// control_editor explore_level (S5; see McpExplore.h): the session and the call. start makes a session for the running
// game (only one this server started: McpPlaySession), samples the nav mesh for islands, and explores; continue explores
// more; read answers with the archive; stop ends it. Each exploring call runs at most its seconds, one step per editor
// frame, with the window held on screen without focus, then answers.
#include "Domains/Play/McpExplore.h"

#include "Containers/Ticker.h"
#include "Domains/Log/McpLogHistory.h"
#include "Domains/Log/McpScriptErrors.h"
#include "Domains/Navigation/McpNavPath.h"
#include "Domains/Play/McpPlaySession.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Solvers/McpPratt.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "NavigationSystem.h"

namespace McpExplore
{
namespace ExploreParts
{
constexpr int32 MaxTrack = 16;
constexpr int32 NavSamples = 64;
TSharedPtr<FSession> GSession; // one at a time: a second would steer the same pawn
bool GRunning = false;

APawn* ResolvePawn(UWorld* World, const FString& Name)
{
    if (!Name.IsEmpty())
    {
        return Cast<APawn>(FindActorByNameInWorldForMcp(World, Name, false));
    }
    const APlayerController* Player = World->GetFirstPlayerController();
    return Player ? Player->GetPawn() : nullptr;
}

// Random nav points; those no path from the start reaches are islands (a sealed room, a roof the level never connects).
void SampleNav(FSession& S, UWorld* World)
{
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!Nav)
    {
        return;
    }
    S.Bounds = Nav->GetNavigableWorldBounds();
    for (int32 Index = 0; Index < NavSamples; ++Index)
    {
        FNavLocation Point;
        if (Nav->GetRandomPoint(Point))
        {
            S.NavSamples.Add(Point.Location);
        }
    }
}
} // namespace ExploreParts

using namespace ExploreParts;

FSession* Current(const UWorld* PlayWorld)
{
    const bool bSame = GSession.IsValid() && PlayWorld && GSession->Game.Get() == PlayWorld->GetGameInstance();
    return bSame ? GSession.Get() : nullptr;
}

bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    auto Refuse = [&](const TCHAR* Code, const FString& Message)
    {
        Bridge.SendAutomationResponse(Socket, RequestId, false, Message, nullptr, Code);
        return true;
    };
    UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
    const FString Op = GetJsonStringField(Payload, TEXT("op"));
    if (!World || !McpPlaySession::StartedByMcp(World))
    {
        return Refuse(TEXT("PLAY_NOT_STARTED_BY_MCP"), TEXT("explore_level drives the pawn of a Play session started through this "
                      "server (control_editor play); none runs, or the running one may be someone's game."));
    }
    if (GRunning)
    {
        return Refuse(TEXT("EXPLORE_RUNNING"), TEXT("An explore_level call is still running: wait for its reply."));
    }
    FSession* Session = Current(World);
    if (Op == TEXT("stop") || Op == TEXT("read"))
    {
        if (!Session)
        {
            return Refuse(TEXT("NO_SESSION"), TEXT("No exploration runs in this game: start one with op start."));
        }
        const TSharedPtr<FJsonObject> Result = Report(*Session, GetJsonBoolField(Payload, TEXT("detail"), false));
        if (Op == TEXT("stop"))
        {
            GSession.Reset();
        }
        Bridge.SendAutomationResponse(Socket, RequestId, true, Op == TEXT("stop") ? TEXT("Exploration ended.") : TEXT("Exploration read."), Result);
        return true;
    }
    if (World->IsPaused())
    {
        return Refuse(TEXT("GAME_PAUSED"), TEXT("The running game is paused: resume it (control_editor play, control resume, unpauseGame true)."));
    }
    if (Op == TEXT("start"))
    {
        TSharedPtr<FSession> Fresh = MakeShared<FSession>();
        APawn* Pawn = ResolvePawn(World, GetJsonStringField(Payload, TEXT("actorName")));
        if (!Pawn || !Pawn->GetController())
        {
            return Refuse(TEXT("NO_PAWN"), TEXT("No controlled pawn to drive: the player has none, or actorName names no pawn with a controller."));
        }
        for (const FString& Text : McpHandlerUtils::GetStringArrayField(Payload, TEXT("track")))
        {
            TSharedPtr<McpPratt::FNode> Root;
            FString Why;
            if (Fresh->Track.Num() >= MaxTrack || !McpPratt::Parse(Text, Root, Why))
            {
                return Refuse(TEXT("INVALID_ARGUMENT"), Fresh->Track.Num() >= MaxTrack ? FString::Printf(TEXT("track takes %d texts at most."), MaxTrack)
                                                                                         : FString::Printf(TEXT("track '%s': %s"), *Text, *Why));
            }
            Fresh->Track.Add(Root);
            Fresh->TrackTexts.Add(Text);
        }
        Fresh->Game = World->GetGameInstance();
        Fresh->Pawn = Pawn;
        Fresh->CellCm = FMath::Clamp(GetJsonNumberField(Payload, TEXT("cellCm"), 200.0), 50.0, 2000.0);
        Fresh->HeightCm = FMath::Clamp(GetJsonNumberField(Payload, TEXT("heightCm"), 100.0), 25.0, 1000.0);
        Fresh->Seed = static_cast<uint32>(FMath::Clamp(GetJsonNumberField(Payload, TEXT("seed"), 1.0), 1.0, 4294967295.0));
        Fresh->Start = Pawn->GetActorLocation();
        Fresh->KillZ = World->GetWorldSettings() ? World->GetWorldSettings()->KillZ : Fresh->KillZ;
        SampleNav(*Fresh, World);
        FCell& First = Fresh->Cells.AddDefaulted_GetRef();
        First.Where = Fresh->Start;
        First.Key.X = FMath::FloorToInt32(Fresh->Start.X / Fresh->CellCm);
        First.Key.Y = FMath::FloorToInt32(Fresh->Start.Y / Fresh->CellCm);
        First.Key.Z = FMath::FloorToInt32((Fresh->Start.Z - Pawn->GetSimpleCollisionHalfHeight()) / Fresh->HeightCm);
        // The start judged as every later cell is, or the first judgement would file the start again as a new cell.
        First.Key.Flags = Fresh->LastFlags = FlagsNow(*Fresh, World);
        Fresh->Index.Add(First.Key, 0);
        GSession = Fresh;
        Session = GSession.Get();
    }
    else if (Op != TEXT("continue"))
    {
        return Refuse(TEXT("INVALID_ARGUMENT"), TEXT("op is start, continue, read or stop."));
    }
    else if (!Session)
    {
        return Refuse(TEXT("NO_SESSION"), TEXT("No exploration runs in this game: start one with op start."));
    }
    Session->Wanted = static_cast<int32>(GetJsonNumberField(Payload, TEXT("cell"), -1.0));
    const double Until = FPlatformTime::Seconds() + FMath::Clamp(GetJsonNumberField(Payload, TEXT("seconds"), 10.0), 1.0, 22.0);
    const double Since = FMcpLogHistory::Now();
    const FMcpEditorRunHold Hold = BeginEditorRunForMcp();
    GRunning = true;
    TWeakObjectPtr<UEngineWireSubsystem> WeakBridge(&Bridge);
    TWeakPtr<FSession> WeakSession = GSession;
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakBridge, WeakSession, Until, Since, Hold, RequestId, Socket](float) {
        TSharedPtr<FSession> Live = WeakSession.Pin();
        UWorld* Now = GEditor ? GEditor->PlayWorld.Get() : nullptr;
        FString Stop;
        if (Live && WeakBridge.IsValid() && Step(*Live, Now, Until, Stop))
        {
            return true;
        }
        EndEditorRunForMcp(Hold);
        GRunning = false;
        if (Live)
        {
            Park(*Live);
        }
        // Blueprint errors raised while it explored, each once, on the cell the round started from.
        TArray<FString> Messages;
        for (const FString& Line : Live ? FMcpScriptErrors::Get().LinesSince(Since, Messages) : TArray<FString>())
        {
            if (Live->Findings.Num() < MaxFindings && !Live->Findings.ContainsByPredicate([&Line](const FFinding& F) { return F.Text == Line; }))
            {
                Live->Findings.Add({TEXT("script_error"), Line, FVector::ZeroVector, Live->From, Now ? Now->GetTimeSeconds() : 0.0});
            }
        }
        if (UEngineWireSubsystem* Self = WeakBridge.Get())
        {
            TSharedPtr<FJsonObject> Result = Live ? Report(*Live, false) : McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("stopped"), Stop.IsEmpty() ? FString(TEXT("seconds")) : Stop);
            const FString Why = Stop == TEXT("pawn_lost") ? TEXT(" Stopped early: the pawn is gone (destroyed, or out of the world).")
                              : Stop == TEXT("paused")    ? TEXT(" Stopped early: the game paused itself.")
                              : Stop.IsEmpty()            ? TEXT("")
                                                          : TEXT(" Stopped early: Play ended.");
            Self->SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Explored: %d cell(s) in %d round(s), %d finding(s).%s"),
                Live ? Live->Cells.Num() : 0, Live ? Live->Rounds : 0, Live ? Live->Findings.Num() : 0, *Why), Result);
        }
        return false;
    }), 0.0f);
    return true;
}
} // namespace McpExplore

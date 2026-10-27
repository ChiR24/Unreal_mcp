// control_actor settle_physics (S13; see McpSettle.h): the call checks the actors named, starts Simulate In Editor in the
// level viewport, and advances the run once per editor frame: the bodies wake when Simulate has begun play, are sampled
// until they rest or time is up, Simulate ends, and only then are the resting poses kept and the call answered.
#include "Domains/Play/McpSettle.h"

#include "Containers/Ticker.h"
#include "Domains/ControlActor/Placement/Fix/McpPlacementFix.h"
#include "Editor.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/BridgeHelpers/Responses/McpResponseVerify.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "IAssetViewport.h"
#include "LevelEditor.h"

namespace McpSettle
{
namespace SettleParts
{
constexpr int32 MaxActors = 64;
constexpr double StartBy = 12.0; // real seconds for Simulate to begin
constexpr double ReplyBy = 26.0; // real seconds from the call for Simulate to have ended
bool GSettling = false;          // one run at a time: a second would read, and end, this one's Simulate session

// Every way the run ends without keeping anything: Simulate never started or never ended in time, it was stopped in the
// editor while the bodies fell, or physics took none of them.
bool Fail(UEngineWireSubsystem& Bridge, const FRun& Run, const FString& RequestId, const TSharedPtr<FMcpBridgeWebSocket>& Socket)
{
    UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
    if (GEditor && GEditor->IsPlaySessionRequestQueued())
    {
        GEditor->CancelRequestPlaySession();
    }
    else if (World)
    {
        GEditor->RequestEndPlayMap();
    }
    if (Run.EndedBecause == TEXT("nothing_simulated"))
    {
        TSharedPtr<FJsonObject> Detail = McpHandlerUtils::CreateResultObject();
        Detail->SetArrayField(TEXT("skipped"), Run.Skipped);
        Bridge.SendAutomationResponse(Socket, RequestId, false,
            TEXT("Physics simulated none of the actors named: skipped says why for each."), Detail, TEXT("NOTHING_TO_SETTLE"));
        return false;
    }
    if (Run.EndedBecause == TEXT("paused"))
    {
        Bridge.SendAutomationResponse(Socket, RequestId, false, TEXT("The level's game paused itself in Simulate (a title screen or a "
            "pause menu), so nothing fell; nothing was kept. Give the level a game mode that does not pause (manage_level "
            "set_world_settings gameMode), then settle again."), nullptr, TEXT("GAME_PAUSED"));
        return false;
    }
    const bool bStopped = Run.Phase == EPhase::Settling;
    Bridge.SendAutomationResponse(Socket, RequestId, false,
        bStopped ? TEXT("Simulate was stopped in the editor before the bodies came to rest; nothing was kept.")
                 : TEXT("The Simulate session did not start or end in time; nothing was kept."),
        nullptr, bStopped ? TEXT("SIMULATE_STOPPED") : TEXT("SIMULATE_TIMEOUT"));
    return false;
}

// One editor frame of the run; false once it has answered.
bool Step(UEngineWireSubsystem& Bridge, FRun& Run, const FString& RequestId, const TSharedPtr<FMcpBridgeWebSocket>& Socket)
{
    UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
    const double Elapsed = FPlatformTime::Seconds() - Run.StartReal;
    if (Run.Phase == EPhase::Starting && World && World->HasBegunPlay())
    {
        StartBodies(Run);
        Run.Phase = EPhase::Settling;
        Run.SettleStart = Run.SettleEnd = FPlatformTime::Seconds();
        if (!Run.Bodies.ContainsByPredicate([](const FBody& Body) { return Body.bSimulated; }))
        {
            Run.EndedBecause = TEXT("nothing_simulated");
            Run.Phase = EPhase::Ending;
            GEditor->RequestEndPlayMap();
        }
        return true;
    }
    if (Run.Phase == EPhase::Settling && World)
    {
        // A game mode that pauses itself (a title screen) stops physics too: nothing would fall (found with playtest).
        Run.EndedBecause = World->IsPaused() ? FString(TEXT("paused")) : Run.EndedBecause;
        if (World->IsPaused() || Sample(Run, World))
        {
            Run.Phase = EPhase::Ending;
            GEditor->RequestEndPlayMap();
        }
        return true;
    }
    if ((Run.Phase == EPhase::Starting && Elapsed < StartBy) || (Run.Phase == EPhase::Ending && World && Elapsed < ReplyBy))
    {
        return true;
    }
    EndEditorRunForMcp(Run.Hold);
    GSettling = false;
    const bool bNothingToKeep = Run.EndedBecause == TEXT("nothing_simulated") || Run.EndedBecause == TEXT("paused");
    if (Run.Phase != EPhase::Ending || World || bNothingToKeep)
    {
        return Fail(Bridge, Run, RequestId, Socket);
    }
    // The edit happens now, long after the handler returned: inside the request's brackets again, so the reply gets
    // the undo block and the placement checks, and the journal says this server moved the actors.
    Bridge.RunLatentReply(RequestId, TEXT("control_actor settle_physics"), [&Bridge, &Run, &RequestId, &Socket]() {
        FString Code;
        FString Message;
        const int32 Kept = Keep(Run, Code, Message);
        if (Kept == INDEX_NONE)
        {
            Bridge.SendAutomationResponse(Socket, RequestId, false, TEXT("The resting poses were not kept: ") + Message, nullptr, Code);
            return;
        }
        const TSharedPtr<FJsonObject> Result = Reply(Run, Kept, Message);
        Bridge.SendAutomationResponse(Socket, RequestId, true, Message, Result);
    });
    return false;
}

// The actors named, each a body to settle or a row under skipped; false (refused, nothing started) when a name is unknown
// or none can fall.
bool ReadBodies(const TArray<FString>& Names, TFunctionRef<AActor*(const FString&)> Find, FRun& Run,
                TSharedPtr<FJsonObject>& OutDetail, FString& OutCode)
{
    TArray<TSharedPtr<FJsonValue>> Missing;
    for (const FString& Name : Names)
    {
        AActor* Actor = Find(Name);
        FString Why = Actor ? McpPlacementFix::PinnedReason(Actor) : FString();
        if (Actor && Why.IsEmpty() && !HasPhysicsBody(Actor))
        {
            Why = TEXT("its root is no body with physics collision, so nothing of it falls");
        }
        if (!Actor)
        {
            Missing.Add(MakeShared<FJsonValueString>(Name));
        }
        else if (!Why.IsEmpty())
        {
            Skip(Run, McpActorRef(Actor), Why);
        }
        else if (!Run.Bodies.ContainsByPredicate([Actor](const FBody& Body) { return Body.Editor.Get() == Actor; }))
        {
            FBody& Body = Run.Bodies.AddDefaulted_GetRef();
            Body.Editor = Actor;
            Body.Name = McpActorRef(Actor);
            Body.Start = Body.End = Actor->GetActorTransform();
        }
    }
    OutDetail = McpHandlerUtils::CreateResultObject();
    if (Missing.Num() > 0)
    {
        OutDetail->SetArrayField(TEXT("missing"), Missing);
        OutCode = TEXT("ACTOR_NOT_FOUND");
    }
    else if (Run.Bodies.Num() == 0)
    {
        OutDetail->SetArrayField(TEXT("skipped"), Run.Skipped);
        OutCode = TEXT("NOTHING_TO_SETTLE");
    }
    return OutCode.IsEmpty();
}
} // namespace SettleParts

using namespace SettleParts;

bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find)
{
    const auto Refuse = [&](const TCHAR* Code, const FString& Message, const TSharedPtr<FJsonObject>& Detail) {
        Bridge.SendAutomationResponse(Socket, RequestId, false, Message, Detail, Code);
        return true;
    };
    if (!GEditor || GEditor->IsPlaySessionInProgress() || GSettling)
    {
        return GSettling ? Refuse(TEXT("SETTLE_RUNNING"), TEXT("A settle_physics run is still simulating: wait for its reply."), nullptr)
                         : Refuse(TEXT("PLAY_RUNNING"), TEXT("settle_physics runs a Simulate session of its own: stop the running "
                                                             "Play or Simulate first."), nullptr);
    }
    const TArray<FString> Names = McpHandlerUtils::GetStringArrayField(Payload, TEXT("actorNames"));
    if (Names.Num() == 0 || Names.Num() > MaxActors)
    {
        return Refuse(TEXT("INVALID_ARGUMENT"), FString::Printf(TEXT("actorNames lists 1 to %d actors to settle."), MaxActors), nullptr);
    }
    const TSharedPtr<FRun> Run = MakeShared<FRun>();
    TSharedPtr<FJsonObject> Detail;
    FString Code;
    if (!ReadBodies(Names, Find, *Run, Detail, Code))
    {
        return Refuse(*Code, Code == TEXT("ACTOR_NOT_FOUND") ? TEXT("Some actorNames match no actor in the level; nothing was simulated.")
                                                             : TEXT("None of the actors named can fall: skipped says why for each."), Detail);
    }
    TSharedPtr<IAssetViewport> Viewport;
    if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>(TEXT("LevelEditor")))
    {
        Viewport = LevelEditor->GetFirstActiveViewport();
    }
    if (!Viewport.IsValid())
    {
        return Refuse(TEXT("NO_LEVEL_VIEWPORT"), TEXT("Simulate runs in a level viewport, and the editor has none open."), nullptr);
    }
    Run->Seconds = FMath::Clamp(GetJsonNumberField(Payload, TEXT("seconds"), 3.0), 0.5, 10.0);
    Run->FallCm = FMath::Clamp(GetJsonNumberField(Payload, TEXT("fallCm"), 500.0), 10.0, 100000.0);
    Run->bDryRun = GetJsonBoolField(Payload, TEXT("dryRun"), false);
    Run->StartReal = FPlatformTime::Seconds();
    // No play settings given: the editor copies its own before Simulate starts and changes only the copy (Simulate
    // forces one standalone client), so the user's Play settings stay as they were.
    FRequestPlaySessionParams Params;
    Params.WorldType = EPlaySessionWorldType::SimulateInEditor;
    Params.DestinationSlateViewport = Viewport;
    GEditor->RequestPlaySession(Params);
    Run->Hold = BeginEditorRunForMcp(); // a minimized editor simulates at about 3 fps
    GSettling = true;
    TWeakObjectPtr<UEngineWireSubsystem> WeakBridge(&Bridge);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakBridge, Run, RequestId, Socket](float) {
        UEngineWireSubsystem* Self = WeakBridge.Get();
        GSettling = GSettling && Self;
        return Self && Step(*Self, *Run, RequestId, Socket);
    }), 0.0f);
    return true;
}
} // namespace McpSettle

#include "Domains/Play/McpPlaytest.h"

#include "AIController.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Domains/Log/McpLogHistory.h"
#include "Domains/Navigation/McpNavPath.h"
#include "Editor.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/Solvers/McpPratt.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Navigation/PathFollowingComponent.h" // EPathFollowingRequestResult: AIController.h only declares it from 5.x on

namespace McpPlaytest
{
namespace PlaytestParts
{
constexpr int32 MaxChecks = 16;
bool GRunning = false; // one leg at a time: two would steer the same game

APawn* ResolvePawn(UWorld* World, const FString& Name)
{
    if (!Name.IsEmpty())
    {
        return Cast<APawn>(FindActorByNameInWorldForMcp(World, Name, false));
    }
    const APlayerController* Player = World->GetFirstPlayerController();
    return Player ? Player->GetPawn() : nullptr;
}

bool ReadGoal(const TSharedPtr<FJsonObject>& Payload, UWorld* World, FVector& Out, FString& OutError)
{
    FString Name;
    if (Payload->TryGetStringField(TEXT("goalActor"), Name) && !Name.IsEmpty())
    {
        const AActor* Actor = FindActorByNameInWorldForMcp(World, Name, false);
        Out = Actor ? Actor->GetActorLocation() : FVector::ZeroVector;
        OutError = Actor ? FString() : FString::Printf(TEXT("goalActor '%s' names no actor in the running game."), *Name);
        return Actor != nullptr;
    }
    OutError = TEXT("Give goal (a point) or goalActor.");
    Out = ExtractVectorField(Payload, TEXT("goal"), FVector::ZeroVector);
    return Payload->HasField(TEXT("goal"));
}

bool ReadChecks(const TSharedPtr<FJsonObject>& Payload, FRun& Run, FString& OutError)
{
    for (const FString& Text : McpHandlerUtils::GetStringArrayField(Payload, TEXT("checks")))
    {
        TSharedPtr<McpPratt::FNode> Root;
        FString Why;
        if (Run.Checks.Num() >= MaxChecks || !McpPratt::Parse(Text, Root, Why))
        {
            OutError = Run.Checks.Num() >= MaxChecks ? FString::Printf(TEXT("checks takes %d assertions at most."), MaxChecks)
                                                     : FString::Printf(TEXT("check '%s': %s"), *Text, *Why);
            return false;
        }
        Run.Checks.Add(Root);
        Run.CheckTexts.Add(Text);
    }
    return true;
}
} // namespace PlaytestParts

using namespace PlaytestParts;

bool Handle(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    auto Refuse = [&](const TCHAR* Code, const FString& Message)
    {
        Bridge.SendAutomationResponse(Socket, RequestId, false, Message, nullptr, Code);
        return true;
    };
    UWorld* World = GEditor ? GEditor->PlayWorld.Get() : nullptr;
    if (!World)
    {
        return Refuse(TEXT("PIE_NOT_RUNNING"), TEXT("No game is running: start one with control_editor play, then run the leg."));
    }
    if (GRunning)
    {
        return Refuse(TEXT("PLAYTEST_RUNNING"), TEXT("A playtest leg is already running: wait for its reply."));
    }
    if (!McpPlaySession::StartedByMcp(World))
    {
        return Refuse(TEXT("PLAY_NOT_STARTED_BY_MCP"), TEXT("The running game was started in the editor, not through this server: someone "
                      "may be playing it, so nothing is driven. Stop it and start Play with control_editor play."));
    }
    // A leg in a paused world sat out its whole time limit and came back inconclusive, with nothing having moved.
    if (World->IsPaused())
    {
        return Refuse(TEXT("GAME_PAUSED"), TEXT("The running game is paused (its own pause menu or title screen, or control_editor "
                      "pause), so nothing would move: resume it (control_editor play with control resume and unpauseGame true), then run the leg."));
    }
    TSharedPtr<FRun> Run = MakeShared<FRun>();
    APawn* Pawn = ResolvePawn(World, GetJsonStringField(Payload, TEXT("actorName")));
    AController* Controller = Pawn ? Pawn->GetController() : nullptr;
    FString Error;
    if (!Controller)
    {
        return Refuse(TEXT("NO_PAWN"), TEXT("No controlled pawn to drive: the player has none, or actorName names no pawn with a controller."));
    }
    if (!ReadGoal(Payload, World, Run->Goal, Error) || !ReadChecks(Payload, *Run, Error))
    {
        return Refuse(TEXT("INVALID_ARGUMENT"), Error);
    }
    Run->Start = Run->Last = Pawn->GetActorLocation();
    // The way first: with no full path on the nav mesh the leg is not tried, and that is no verdict on the game.
    McpNavigationHandlers::FMcpNavPath Path;
    FString Code;
    if (!McpNavigationHandlers::FindNavPath(World, Run->Start, Run->Goal, FVector(100.0, 100.0, 250.0), Path, Code, Error))
    {
        return Refuse(*Code, Error);
    }
    if (Path.Reason != TEXT("ok"))
    {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("verdict"), TEXT("inconclusive"));
        Result->SetStringField(TEXT("ended"), Path.Reason);
        Bridge.SendAutomationResponse(Socket, RequestId, true, FString::Printf(TEXT("Inconclusive: the nav mesh gives no full path to the "
                                      "goal (%s), so the leg was not tried."), *Path.Reason), Result);
        return true;
    }
    Run->World = World;
    Run->Pawn = Pawn;
    Run->Controller = Controller;
    Run->AcceptRadius = FMath::Clamp(GetJsonNumberField(Payload, TEXT("acceptRadius"), 100.0), 10.0, 2000.0);
    Run->StuckSeconds = FMath::Clamp(GetJsonNumberField(Payload, TEXT("stuckSeconds"), 2.0), 0.5, 20.0);
    Run->FallCm = FMath::Clamp(GetJsonNumberField(Payload, TEXT("fallCm"), 500.0), 50.0, 100000.0);
    Run->MaxGameSeconds = FMath::Clamp(GetJsonNumberField(Payload, TEXT("maxSeconds"), 15.0), 1.0, 60.0);
    Run->BestDistance = FVector::Dist2D(Run->Start, Run->Goal);
    Run->StartGame = World->GetTimeSeconds();
    Run->StartReal = FPlatformTime::Seconds();
    Run->LogSince = FMcpLogHistory::Now();
    if (APlayerController* Player = Cast<APlayerController>(Controller))
    {
        UAIBlueprintHelperLibrary::SimpleMoveToLocation(Player, Run->Goal);
    }
    else if (AAIController* AI = Cast<AAIController>(Controller))
    {
        // A move refused outright read as "stuck" two seconds later, though nothing had been driven.
        if (AI->MoveToLocation(Run->Goal, static_cast<float>(Run->AcceptRadius * 0.5)) == EPathFollowingRequestResult::Failed)
        {
            return Refuse(TEXT("MOVE_FAILED"), TEXT("The pawn's AI controller refused the move (its path following could not start), so "
                          "nothing was driven."));
        }
    }
    // A minimized editor runs Play at about 3 fps: held on screen without focus for the leg, as sample_motion does.
    Run->Hold = BeginEditorRunForMcp();
    GRunning = true;
    TWeakObjectPtr<UEngineWireSubsystem> WeakBridge(&Bridge);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakBridge, Run, RequestId, Socket](float) -> bool {
        const FString Ended = Advance(*Run);
        if (Ended.IsEmpty() && WeakBridge.IsValid())
        {
            return true;
        }
        if (AController* Driver = Run->Controller.Get())
        {
            Driver->StopMovement();
        }
        EndEditorRunForMcp(Run->Hold);
        GRunning = false;
        FString Verdict;
        FString Message;
        const TSharedPtr<FJsonObject> Result = Judge(*Run, Ended.IsEmpty() ? FString(TEXT("world_ended")) : Ended, Verdict, Message);
        if (UEngineWireSubsystem* Self = WeakBridge.Get())
        {
            Self->SendAutomationResponse(Socket, RequestId, true, Message, Result);
        }
        return false;
    }), 0.0f);
    return true;
}
} // namespace McpPlaytest

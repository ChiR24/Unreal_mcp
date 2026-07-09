#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_REPLAY_API

#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceReplayInternal.h"

#include "Engine/DemoNetDriver.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "McpAutomationBridgeSubsystem.h"
#include "ReplaySubsystem.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"

namespace McpSequenceRecordReplay
{
bool HandleConfigureKillcamDuration(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    double Duration = GMcpReplaySettings.KillcamDurationSeconds;
    if (!Payload.IsValid() || !Payload->TryGetNumberField(TEXT("durationSeconds"), Duration) ||
        !FMath::IsFinite(Duration) || Duration <= 0.0)
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("durationSeconds must be greater than zero"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    GMcpReplaySettings.KillcamDurationSeconds = static_cast<float>(Duration);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("durationSeconds"), Duration);
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Killcam duration configured"), Result);
    return true;
}

namespace
{
struct FReplaySeekWaitState
{
    bool bCompleted = false;
    FTSTicker::FDelegateHandle TickerHandle;
};

void CancelReplaySeek(TSharedRef<FReplaySeekWaitState> State)
{
    if (State->bCompleted) return;
    State->bCompleted = true;
    if (State->TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(State->TickerHandle);
        State->TickerHandle = FTSTicker::FDelegateHandle();
    }
}

bool PlayDemo(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld* World, UReplaySubsystem* Replay)
{
    const FString Name = GetReplayName(Payload);
    if (Name.IsEmpty())
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("replayName is required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (Replay->IsRecording())
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("Stop demo recording before starting playback"), TEXT("ALREADY_RECORDING"));
        return true;
    }
    if (!Replay->PlayReplay(Name, World, GetReplayOptions(Payload)))
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("Replay subsystem failed to start demo playback"), TEXT("PLAYBACK_FAILED"));
        return true;
    }
    if (AWorldSettings* Settings = World->GetWorldSettings())
    {
        Settings->DemoPlayTimeDilation = GMcpReplaySettings.PlaybackSpeed;
    }
    Subsystem->SendAutomationResponse(Socket, RequestId, true, TEXT("Demo playback started"), MakeReplayState(World, Replay));
    return true;
}

bool EnsureKillcamReplay(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld* World, UReplaySubsystem* Replay)
{
    if (Replay->IsPlaying()) return true;
    const FString Name = GetReplayName(Payload);
    if (Name.IsEmpty() || !Replay->PlayReplay(Name, World, GetReplayOptions(Payload)))
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("Killcam requires an active replay or a playable replayName"), TEXT("NOT_AVAILABLE"));
        return false;
    }
    return true;
}

bool PauseDemo(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld* World, UReplaySubsystem* Replay, UDemoNetDriver* Driver)
{
    bool bPause = !Driver->GetChannelsArePaused();
    if (Payload.IsValid()) Payload->TryGetBoolField(TEXT("paused"), bPause);
    Driver->PauseChannels(bPause);
    if (AWorldSettings* Settings = World->GetWorldSettings())
    {
        APlayerController* PC = Driver->GetSpectatorController();
        Settings->SetPauserPlayerState(bPause && PC ? PC->PlayerState : nullptr);
    }
    Subsystem->SendAutomationResponse(Socket, RequestId, true, bPause ? TEXT("Demo playback paused") : TEXT("Demo playback resumed"), MakeReplayState(World, Replay));
    return true;
}

bool SetDemoSpeed(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld* World, UReplaySubsystem* Replay)
{
    double Speed = 0.0;
    if (!Payload.IsValid() || (!Payload->TryGetNumberField(TEXT("speed"), Speed) && !Payload->TryGetNumberField(TEXT("playbackSpeed"), Speed)) ||
        !FMath::IsFinite(Speed) || Speed <= 0.0)
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("speed must be greater than zero"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    GMcpReplaySettings.PlaybackSpeed = static_cast<float>(Speed);
    World->GetWorldSettings()->DemoPlayTimeDilation = GMcpReplaySettings.PlaybackSpeed;
    Subsystem->SendAutomationResponse(Socket, RequestId, true, TEXT("Demo playback speed updated"), MakeReplayState(World, Replay));
    return true;
}

bool SeekDemo(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld* World, UReplaySubsystem* Replay, UDemoNetDriver* Driver)
{
    double TimeSeconds = 0.0;
    if (Action == TEXT("start_killcam"))
    {
        double Duration = GMcpReplaySettings.KillcamDurationSeconds;
        if (Payload.IsValid()) Payload->TryGetNumberField(TEXT("durationSeconds"), Duration);
        const double TotalTime = Driver->GetDemoTotalTime();
        const double CurrentTime = Driver->GetDemoCurrentTime();
        TimeSeconds = FMath::Max(0.0, (TotalTime > 0.0 ? TotalTime : CurrentTime) - Duration);
    }
    else if (!Payload.IsValid() || (!Payload->TryGetNumberField(TEXT("timeSeconds"), TimeSeconds) && !Payload->TryGetNumberField(TEXT("seconds"), TimeSeconds)) ||
             !FMath::IsFinite(TimeSeconds) || TimeSeconds < 0.0)
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("timeSeconds must be zero or greater"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const double TotalTime = Driver->GetDemoTotalTime();
    const double TargetTime = TotalTime > 0.0
        ? FMath::Clamp(TimeSeconds, 0.0, TotalTime)
        : TimeSeconds;
    constexpr double SeekToleranceSeconds = 0.25;
    constexpr double SeekTimeoutSeconds = 10.0;
    const double Deadline = FPlatformTime::Seconds() + SeekTimeoutSeconds;
    TSharedRef<FReplaySeekWaitState> State =
        MakeShared<FReplaySeekWaitState>();
    if (!Subsystem->RegisterAutomationRequestCancellation(
            RequestId, [State]() { CancelReplaySeek(State); }))
    {
        CancelReplaySeek(State);
        return true;
    }
    Driver->GotoTimeInSeconds(static_cast<float>(TargetTime));
    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(Subsystem);
    TWeakObjectPtr<UWorld> WeakWorld(World);
    TWeakObjectPtr<UReplaySubsystem> WeakReplay(Replay);
    TWeakObjectPtr<UDemoNetDriver> WeakDriver(Driver);
    State->TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda(
            [State, WeakSubsystem, WeakWorld, WeakReplay, WeakDriver,
             RequestId, Socket, Action, TimeSeconds, TargetTime,
             Deadline, SeekToleranceSeconds](float)
            {
                if (State->bCompleted) return false;
                UMcpAutomationBridgeSubsystem* CurrentSubsystem =
                    WeakSubsystem.Get();
                UWorld* CurrentWorld = WeakWorld.Get();
                UReplaySubsystem* CurrentReplay = WeakReplay.Get();
                UDemoNetDriver* CurrentDriver = WeakDriver.Get();
                if (!CurrentSubsystem || !CurrentWorld || !CurrentReplay ||
                    !CurrentDriver)
                {
                    State->bCompleted = true;
                    if (CurrentSubsystem)
                    {
                        CurrentSubsystem->SendAutomationError(
                            Socket, RequestId,
                            TEXT("Replay playback ended before the seek completed"),
                            TEXT("REPLAY_SEEK_FAILED"));
                    }
                    return false;
                }
                const double CurrentTime =
                    CurrentDriver->GetDemoCurrentTime();
                if (FMath::Abs(CurrentTime - TargetTime) <=
                    SeekToleranceSeconds)
                {
                    State->bCompleted = true;
                    TSharedPtr<FJsonObject> Result =
                        MakeReplayState(CurrentWorld, CurrentReplay);
                    Result->SetNumberField(
                        TEXT("requestedTimeSeconds"), TimeSeconds);
                    Result->SetNumberField(
                        TEXT("targetTimeSeconds"), TargetTime);
                    Result->SetNumberField(
                        TEXT("actualTimeSeconds"), CurrentTime);
                    Result->SetNumberField(
                        TEXT("seekToleranceSeconds"), SeekToleranceSeconds);
                    Result->SetBoolField(TEXT("seekCompleted"), true);
                    Result->SetBoolField(TEXT("seekPending"), false);
                    CurrentSubsystem->SendAutomationResponse(
                        Socket, RequestId, true,
                        Action == TEXT("start_killcam")
                            ? TEXT("Killcam playback reached its target")
                            : TEXT("Demo seek completed"),
                        Result);
                    return false;
                }
                if (FPlatformTime::Seconds() >= Deadline)
                {
                    State->bCompleted = true;
                    TSharedPtr<FJsonObject> Result =
                        MakeReplayState(CurrentWorld, CurrentReplay);
                    Result->SetNumberField(
                        TEXT("requestedTimeSeconds"), TimeSeconds);
                    Result->SetNumberField(
                        TEXT("targetTimeSeconds"), TargetTime);
                    Result->SetNumberField(
                        TEXT("actualTimeSeconds"), CurrentTime);
                    Result->SetBoolField(TEXT("seekCompleted"), false);
                    Result->SetBoolField(TEXT("seekPending"), false);
                    CurrentSubsystem->SendAutomationResponse(
                        Socket, RequestId, false,
                        TEXT("Timed out waiting for replay seek completion"),
                        Result, TEXT("REPLAY_SEEK_TIMEOUT"));
                    return false;
                }
                return true;
            }),
        0.05f);
    return true;
}
}

bool HandleReplayPlaybackAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UWorld* World, UReplaySubsystem* Replay)
{
    if (Action == TEXT("play_demo")) return PlayDemo(Subsystem, RequestId, Payload, RequestingSocket, World, Replay);
    if (Action == TEXT("start_killcam") &&
        Payload.IsValid() && Payload->HasField(TEXT("durationSeconds")))
    {
        double Duration = 0.0;
        if (!Payload->TryGetNumberField(TEXT("durationSeconds"), Duration) ||
            !FMath::IsFinite(Duration) || Duration <= 0.0)
        {
            Subsystem->SendAutomationError(
                RequestingSocket, RequestId,
                TEXT("durationSeconds must be greater than zero"),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }
    UDemoNetDriver* Driver = nullptr;
    if (Action == TEXT("start_killcam") && !EnsureKillcamReplay(Subsystem, RequestId, Payload, RequestingSocket, World, Replay)) return true;
    if (!RequirePlayback(Subsystem, RequestId, RequestingSocket, World, Replay, Driver)) return true;
    if (Action == TEXT("pause_demo")) return PauseDemo(Subsystem, RequestId, Payload, RequestingSocket, World, Replay, Driver);
    if (Action == TEXT("set_demo_playback_speed")) return SetDemoSpeed(Subsystem, RequestId, Payload, RequestingSocket, World, Replay);
    return SeekDemo(Subsystem, RequestId, Action, Payload, RequestingSocket, World, Replay, Driver);
}
}

#endif

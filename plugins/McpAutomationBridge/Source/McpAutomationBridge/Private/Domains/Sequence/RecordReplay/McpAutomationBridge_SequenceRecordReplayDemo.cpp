#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceRecordReplay.h"

#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceReplayInternal.h"
#include "McpAutomationBridgeSubsystem.h"

namespace McpSequenceRecordReplay
{
bool HandleDemoReplayAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (!IsDemoReplayAction(Action))
    {
        return false;
    }

#if !MCP_HAS_REPLAY_API
    Subsystem->SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Demo replay actions are unavailable because this Unreal Engine "
             "build does not provide ReplaySubsystem."),
        TEXT("NOT_AVAILABLE"));
    return true;
#else
    if (Payload.IsValid())
    {
        FString ValidationError;
        if (!ValidateReplayRequest(Payload, ValidationError))
        {
            Subsystem->SendAutomationError(
                RequestingSocket, RequestId, ValidationError,
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }

    if (Action == TEXT("configure_killcam_duration"))
    {
        return HandleConfigureKillcamDuration(
            Subsystem, RequestId, Payload, RequestingSocket);
    }

    UWorld* World = nullptr;
    UReplaySubsystem* Replay = nullptr;
    if (Action == TEXT("configure_demo_settings"))
    {
        // Settings-only (catalogued for edit mode, dogfood #129): store them now and
        // apply to the live replay driver only when a PIE/game world already exists.
        World = GetRuntimeWorld();
        Replay = GetReplaySubsystem(World);
        return HandleReplayRecordingAction(
            Subsystem, RequestId, Action, Payload, RequestingSocket, World, Replay);
    }
    if (!RequireRuntime(Subsystem, RequestId, RequestingSocket, World, Replay))
    {
        return true;
    }

    if (HandleReplayRecordingAction(
            Subsystem, RequestId, Action, Payload, RequestingSocket,
            World, Replay))
    {
        return true;
    }

    return HandleReplayPlaybackAction(
        Subsystem, RequestId, Action, Payload, RequestingSocket, World, Replay);
#endif
}
}

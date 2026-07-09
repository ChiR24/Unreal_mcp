#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_REPLAY_API

#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceReplayInternal.h"

#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "ReplaySubsystem.h"

namespace McpSequenceRecordReplay
{
namespace
{
bool ConfigureDemoSettings(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld* World, UReplaySubsystem* Replay)
{
    FMcpReplaySettings Updated = GMcpReplaySettings;
    FString Value = GetReplayName(Payload);
    if (!Value.IsEmpty()) Updated.DefaultReplayName = Value;
    Value = McpHandlerUtils::GetOptionalString(Payload, TEXT("friendlyName"));
    if (!Value.IsEmpty()) Updated.FriendlyName = Value;
    TArray<FString> Options = GetReplayStringArray(Payload, TEXT("additionalOptions"));
    if (Options.Num() > 0) Updated.AdditionalOptions = MoveTemp(Options);

    double Number = 0.0;
    if (Payload->TryGetNumberField(TEXT("checkpointSaveMaxMSPerFrame"), Number))
    {
        Updated.CheckpointSaveMaxMSPerFrame = static_cast<float>(Number);
    }
    if (Payload->TryGetNumberField(TEXT("maxRecordTimeSeconds"), Number))
    {
        Updated.MaxRecordTimeSeconds = static_cast<float>(Number);
    }
    if (Payload->TryGetNumberField(TEXT("playbackSpeed"), Number))
    {
        Updated.PlaybackSpeed = static_cast<float>(Number);
    }
    Payload->TryGetBoolField(TEXT("prioritizeActors"), Updated.bPrioritizeActors);
    Payload->TryGetBoolField(TEXT("loadDefaultMapOnStop"), Updated.bLoadDefaultMapOnStop);
    GMcpReplaySettings = MoveTemp(Updated);
    ApplyDriverSettings(World, Replay);

    TSharedPtr<FJsonObject> Result = MakeReplayState(World, Replay);
    Result->SetNumberField(TEXT("killcamDurationSeconds"), GMcpReplaySettings.KillcamDurationSeconds);
    Result->SetNumberField(TEXT("maxRecordTimeSeconds"), GMcpReplaySettings.MaxRecordTimeSeconds);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, TEXT("Demo replay settings configured"), Result);
    return true;
}

bool StartDemoRecording(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld* World, UReplaySubsystem* Replay)
{
    if (Replay->IsPlaying())
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("Cannot record while a demo replay is playing"), TEXT("ALREADY_PLAYING"));
        return true;
    }
    if (Replay->IsRecording())
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("A demo replay is already recording"), TEXT("ALREADY_RECORDING"));
        return true;
    }

    FString Name = GetReplayName(Payload);
    if (Name.IsEmpty()) Name = GMcpReplaySettings.DefaultReplayName;
    if (Name.IsEmpty())
    {
        Name = FString::Printf(TEXT("McpReplay_%lld"), static_cast<long long>(FDateTime::UtcNow().ToUnixTimestamp()));
    }
    FString FriendlyName = McpHandlerUtils::GetOptionalString(Payload, TEXT("friendlyName"));
    if (FriendlyName.IsEmpty()) FriendlyName = GMcpReplaySettings.FriendlyName.IsEmpty() ? Name : GMcpReplaySettings.FriendlyName;

    Replay->RecordReplay(Name, FriendlyName, GetReplayOptions(Payload), nullptr);
    ApplyDriverSettings(World, Replay);
    if (!Replay->IsRecording())
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("Replay subsystem failed to start demo recording"), TEXT("NOT_AVAILABLE"));
        return true;
    }
    Subsystem->SendAutomationResponse(Socket, RequestId, true, TEXT("Demo recording started"), MakeReplayState(World, Replay));
    return true;
}

bool StopDemoRecording(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, UReplaySubsystem* Replay)
{
    if (!Replay->IsRecording())
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("No demo replay is currently recording"), TEXT("NOT_RECORDING"));
        return true;
    }
    const FString ReplayName = Replay->GetActiveReplayName();
    Replay->StopReplay();
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("replayName"), ReplayName);
    Result->SetBoolField(TEXT("recording"), false);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, TEXT("Demo recording stopped"), Result);
    return true;
}
}

bool HandleReplayRecordingAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UWorld* World, UReplaySubsystem* Replay)
{
    if (Action == TEXT("configure_demo_settings"))
    {
        return ConfigureDemoSettings(Subsystem, RequestId, Payload, RequestingSocket, World, Replay);
    }
    if (Action == TEXT("start_demo_recording"))
    {
        return StartDemoRecording(Subsystem, RequestId, Payload, RequestingSocket, World, Replay);
    }
    if (Action == TEXT("stop_demo_recording"))
    {
        return StopDemoRecording(Subsystem, RequestId, RequestingSocket, Replay);
    }
    return false;
}
}

#endif

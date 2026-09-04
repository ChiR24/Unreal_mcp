#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_REPLAY_API

#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceReplayInternal.h"

#include "Engine/DemoNetDriver.h"
#include "Engine/World.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/ConfigCacheIni.h"
#include "ReplaySubsystem.h"
#include "UObject/UnrealType.h"

namespace McpSequenceRecordReplay
{
namespace
{
// Outside PIE there is no live UDemoNetDriver to configure, so the config-backed
// checkpoint budget is stored on the driver class defaults (which the next driver
// copies) and in the engine ini. CheckpointSaveMaxMSPerFrame is a private UPROPERTY,
// hence reflection.
bool PersistDemoDriverDefaults()
{
    UDemoNetDriver* Defaults = GetMutableDefault<UDemoNetDriver>();
    FFloatProperty* Property = Defaults
        ? CastField<FFloatProperty>(UDemoNetDriver::StaticClass()->FindPropertyByName(TEXT("CheckpointSaveMaxMSPerFrame")))
        : nullptr;
    if (!Property)
    {
        return false;
    }
    Property->SetPropertyValue_InContainer(Defaults, GMcpReplaySettings.CheckpointSaveMaxMSPerFrame);
    if (GConfig)
    {
        GConfig->SetFloat(TEXT("/Script/Engine.DemoNetDriver"), TEXT("CheckpointSaveMaxMSPerFrame"),
            GMcpReplaySettings.CheckpointSaveMaxMSPerFrame, GEngineIni);
        GConfig->Flush(false, GEngineIni);
    }
    return true;
}

TSharedPtr<FJsonObject> DescribeStoredSettings()
{
    TSharedPtr<FJsonObject> Stored = McpHandlerUtils::CreateResultObject();
    Stored->SetStringField(TEXT("replayName"), GMcpReplaySettings.DefaultReplayName);
    Stored->SetStringField(TEXT("friendlyName"), GMcpReplaySettings.FriendlyName);
    TArray<TSharedPtr<FJsonValue>> Options;
    for (const FString& Option : GMcpReplaySettings.AdditionalOptions)
    {
        Options.Add(MakeShared<FJsonValueString>(Option));
    }
    Stored->SetArrayField(TEXT("additionalOptions"), Options);
    Stored->SetNumberField(TEXT("checkpointSaveMaxMSPerFrame"), GMcpReplaySettings.CheckpointSaveMaxMSPerFrame);
    Stored->SetNumberField(TEXT("maxRecordTimeSeconds"), GMcpReplaySettings.MaxRecordTimeSeconds);
    Stored->SetNumberField(TEXT("playbackSpeed"), GMcpReplaySettings.PlaybackSpeed);
    Stored->SetNumberField(TEXT("killcamDurationSeconds"), GMcpReplaySettings.KillcamDurationSeconds);
    Stored->SetBoolField(TEXT("prioritizeActors"), GMcpReplaySettings.bPrioritizeActors);
    Stored->SetBoolField(TEXT("loadDefaultMapOnStop"), GMcpReplaySettings.bLoadDefaultMapOnStop);
    return Stored;
}

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
    const bool bRuntimeAvailable = World != nullptr && Replay != nullptr;
    if (bRuntimeAvailable)
    {
        ApplyDriverSettings(World, Replay);
    }
    const bool bPersisted = PersistDemoDriverDefaults();

    TSharedPtr<FJsonObject> Result = bRuntimeAvailable
        ? MakeReplayState(World, Replay)
        : McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("killcamDurationSeconds"), GMcpReplaySettings.KillcamDurationSeconds);
    Result->SetNumberField(TEXT("maxRecordTimeSeconds"), GMcpReplaySettings.MaxRecordTimeSeconds);
    Result->SetObjectField(TEXT("stored"), DescribeStoredSettings());
    Result->SetBoolField(TEXT("runtimeAvailable"), bRuntimeAvailable);
    Result->SetBoolField(TEXT("appliedToRuntime"), bRuntimeAvailable);
    Result->SetBoolField(TEXT("persistedToConfig"), bPersisted);
    Subsystem->SendAutomationResponse(
        Socket, RequestId, true,
        bRuntimeAvailable ? TEXT("Demo replay settings configured") : TEXT("Demo replay settings stored (applied when a PIE/game world starts)"),
        Result);
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

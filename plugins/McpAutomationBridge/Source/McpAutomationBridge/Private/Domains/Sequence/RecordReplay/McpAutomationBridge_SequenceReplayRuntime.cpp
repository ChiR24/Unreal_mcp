#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceReplayInternal.h"

#if MCP_HAS_REPLAY_API

#include "Engine/DemoNetDriver.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "GameFramework/WorldSettings.h"
#include "McpAutomationBridgeSubsystem.h"
#include "ReplaySubsystem.h"

#endif

namespace McpSequenceRecordReplay
{
FMcpReplaySettings GMcpReplaySettings;

bool IsDemoReplayAction(const FString& Action)
{
    return Action == TEXT("start_demo_recording") ||
        Action == TEXT("stop_demo_recording") ||
        Action == TEXT("configure_demo_settings") ||
        Action == TEXT("play_demo") ||
        Action == TEXT("pause_demo") ||
        Action == TEXT("seek_demo") ||
        Action == TEXT("set_demo_playback_speed") ||
        Action == TEXT("configure_killcam_duration") ||
        Action == TEXT("start_killcam");
}

#if MCP_HAS_REPLAY_API

UWorld* GetRuntimeWorld()
{
    if (!GEngine) return nullptr;
    UWorld* GameWorld = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        UWorld* World = Context.World();
        if (!World) continue;
        if (Context.WorldType == EWorldType::PIE) return World;
        if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::GamePreview)
        {
            GameWorld = World;
        }
    }
    return GameWorld;
}

UReplaySubsystem* GetReplaySubsystem(UWorld* World)
{
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    return GameInstance ? GameInstance->GetSubsystem<UReplaySubsystem>() : nullptr;
}

TArray<FString> GetReplayStringArray(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName)
{
    TArray<FString> Result;
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (Payload.IsValid() && Payload->TryGetArrayField(FieldName, Values) && Values)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString StringValue;
            if (Value.IsValid() && Value->TryGetString(StringValue) && !StringValue.IsEmpty())
            {
                Result.Add(StringValue);
            }
        }
    }
    return Result;
}

FString GetReplayName(const TSharedPtr<FJsonObject>& Payload)
{
    FString Name = McpHandlerUtils::GetOptionalString(Payload, TEXT("replayName"));
    if (Name.IsEmpty()) Name = McpHandlerUtils::GetOptionalString(Payload, TEXT("demoName"));
    if (Name.IsEmpty()) Name = McpHandlerUtils::GetOptionalString(Payload, TEXT("name"));
    return Name;
}

TArray<FString> GetReplayOptions(const TSharedPtr<FJsonObject>& Payload)
{
    TArray<FString> Options = GMcpReplaySettings.AdditionalOptions;
    for (const FString& Option : GetReplayStringArray(Payload, TEXT("additionalOptions")))
    {
        Options.AddUnique(Option);
    }
    return Options;
}

bool RequireRuntime(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld*& OutWorld, UReplaySubsystem*& OutReplay)
{
    OutWorld = GetRuntimeWorld();
    if (!OutWorld)
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("Demo replay actions require an active PIE or game world"), TEXT("NOT_IN_PIE"));
        return false;
    }
    OutReplay = GetReplaySubsystem(OutWorld);
    if (!OutReplay)
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("Replay subsystem is unavailable for the active game instance"), TEXT("NOT_AVAILABLE"));
        return false;
    }
    return true;
}

void ApplyDriverSettings(UWorld* World, UReplaySubsystem* Replay)
{
    Replay->bLoadDefaultMapOnStop = GMcpReplaySettings.bLoadDefaultMapOnStop;
    Replay->SetCheckpointSaveMaxMSPerFrame(
        GMcpReplaySettings.CheckpointSaveMaxMSPerFrame);
    if (UDemoNetDriver* Driver = World->GetDemoNetDriver())
    {
        Driver->SetActorPrioritizationEnabled(GMcpReplaySettings.bPrioritizeActors);
        Driver->SetMaxDesiredRecordTimeMS(
            GMcpReplaySettings.MaxRecordTimeSeconds * 1000.0f);
    }
}

TSharedPtr<FJsonObject> MakeReplayState(UWorld* World, UReplaySubsystem* Replay)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("activeReplayName"), Replay->GetActiveReplayName());
    Result->SetBoolField(TEXT("recording"), Replay->IsRecording());
    Result->SetBoolField(TEXT("playing"), Replay->IsPlaying());
    Result->SetNumberField(TEXT("currentTimeSeconds"), Replay->GetReplayCurrentTime());
    UDemoNetDriver* Driver = World->GetDemoNetDriver();
#if MCP_HAS_REPLAY_SUBSYSTEM_TOTAL_TIME
    Result->SetNumberField(TEXT("totalTimeSeconds"), Replay->GetReplayTotalTime());
#else
    Result->SetNumberField(
        TEXT("totalTimeSeconds"),
        Driver ? Driver->GetDemoTotalTime() : 0.0f);
#endif
    if (Driver)
    {
        Result->SetBoolField(TEXT("paused"), Driver->GetChannelsArePaused());
    }
    if (AWorldSettings* WorldSettings = World->GetWorldSettings())
    {
        Result->SetNumberField(TEXT("playbackSpeed"), WorldSettings->DemoPlayTimeDilation);
    }
    return Result;
}

bool RequirePlayback(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld* World, UReplaySubsystem* Replay, UDemoNetDriver*& OutDriver)
{
    OutDriver = World ? World->GetDemoNetDriver() : nullptr;
    if (!Replay->IsPlaying() || !OutDriver)
    {
        Subsystem->SendAutomationError(Socket, RequestId, TEXT("No demo replay is currently playing"), TEXT("NOT_PLAYING"));
        return false;
    }
    return true;
}

#endif

}

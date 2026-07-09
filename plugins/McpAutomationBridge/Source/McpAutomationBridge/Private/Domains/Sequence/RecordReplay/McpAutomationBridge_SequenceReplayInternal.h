#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UDemoNetDriver;
class UMcpAutomationBridgeSubsystem;
class UReplaySubsystem;
class UWorld;

namespace McpSequenceRecordReplay
{
struct FMcpReplaySettings
{
    FString DefaultReplayName;
    FString FriendlyName;
    TArray<FString> AdditionalOptions;
    float CheckpointSaveMaxMSPerFrame = 5.0f;
    float MaxRecordTimeSeconds = 3600.0f;
    float PlaybackSpeed = 1.0f;
    float KillcamDurationSeconds = 8.0f;
    bool bPrioritizeActors = true;
    bool bLoadDefaultMapOnStop = true;
};

extern FMcpReplaySettings GMcpReplaySettings;

bool IsDemoReplayAction(const FString& Action);
UWorld* GetRuntimeWorld();
UReplaySubsystem* GetReplaySubsystem(UWorld* World);
TArray<FString> GetReplayStringArray(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName);
FString GetReplayName(const TSharedPtr<FJsonObject>& Payload);
TArray<FString> GetReplayOptions(const TSharedPtr<FJsonObject>& Payload);
bool IsSafeReplayName(const FString& Name);
bool ValidateReplayOptions(const TArray<FString>& Options, FString& OutError);
bool ValidateReplayRequest(const TSharedPtr<FJsonObject>& Payload, FString& OutError);
bool RequireRuntime(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld*& OutWorld, UReplaySubsystem*& OutReplay);
void ApplyDriverSettings(UWorld* World, UReplaySubsystem* Replay);
TSharedPtr<FJsonObject> MakeReplayState(UWorld* World, UReplaySubsystem* Replay);
bool RequirePlayback(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket, UWorld* World, UReplaySubsystem* Replay, UDemoNetDriver*& OutDriver);

bool HandleConfigureKillcamDuration(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleReplayRecordingAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UWorld* World, UReplaySubsystem* Replay);
bool HandleReplayPlaybackAction(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UWorld* World, UReplaySubsystem* Replay);
}

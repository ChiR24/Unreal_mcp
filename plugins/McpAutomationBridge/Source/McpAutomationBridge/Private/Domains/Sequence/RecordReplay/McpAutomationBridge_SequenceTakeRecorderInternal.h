#pragma once

#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceRecordReplay.h"

#include "Dom/JsonObject.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpAutomationBridgeSubsystem.h"
#include "UObject/StrongObjectPtr.h"

#ifndef MCP_HAS_TAKE_RECORDER
#define MCP_HAS_TAKE_RECORDER 0
#endif

#if WITH_EDITOR && MCP_HAS_TAKE_RECORDER && \
    __has_include("Recorder/TakeRecorderBlueprintLibrary.h") && \
    __has_include("TakeRecorderSource.h") && \
    __has_include("TakeRecorderSources.h")
#define MCP_SEQUENCE_HAS_TAKE_RECORDER_API 1
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "EngineUtils.h"
#include "LevelSequence.h"
#include "Recorder/TakeRecorder.h"
#include "Recorder/TakeRecorderBlueprintLibrary.h"
#include "Recorder/TakeRecorderPanel.h"
#include "TakeRecorderSource.h"
#include "TakeRecorderSources.h"
#include "TakeMetaData.h"
#include "TakePreset.h"
#else
#define MCP_SEQUENCE_HAS_TAKE_RECORDER_API 0
#endif

namespace McpSequenceRecordReplay
{
bool IsTakeRecorderAction(const FString& Action);
void SendTakeRecorderUnavailable(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket);

#if MCP_SEQUENCE_HAS_TAKE_RECORDER_API
TArray<FString> GetTakeRecorderStringArray(const TSharedPtr<FJsonObject>& Payload, const TCHAR* FieldName);
UTakeRecorderPanel* GetPanel(bool bOpen);
AActor* FindTakeRecorderActor(const FString& Name);
struct FPreparedTakeRecorderSources
{
    TArray<AActor*> Actors;
    TArray<UClass*> Classes;
    UClass* ActorSourceClass = nullptr;
    bool bClearSources = false;
};
struct FTakeRecorderActorSourceState
{
    TWeakObjectPtr<UTakeRecorderSource> Source;
    bool bEnabled = false;
    bool bHasReduceKeys = false;
    bool bReduceKeys = false;
    bool bHasRecordParentHierarchy = false;
    bool bRecordParentHierarchy = false;
    bool bHasRecordType = false;
    int64 RecordTypeValue = 0;
};
bool ReadTakeRecorderStringArray(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* FieldName,
    TArray<FString>& OutValues,
    FString& OutError);
bool PrepareTakeRecorderSources(
    UTakeRecorderSources* Sources,
    const TSharedPtr<FJsonObject>& Payload,
    FPreparedTakeRecorderSources& OutPrepared,
    FString& OutCode,
    FString& OutError);
UClass* ResolveTakeRecorderActorSourceClass(FString& OutError);
bool ValidateTakeActorSourceOptions(UClass* ActorSourceClass, const TSharedPtr<FJsonObject>& Payload, FString& OutError);
UTakeRecorderSource* AddTakeActorSource(UClass* SourceClass, AActor* Actor, UTakeRecorderSources* Sources);
bool ConfigureActorSource(UTakeRecorderSource* Source, const TSharedPtr<FJsonObject>& Payload, int32& OutAppliedOptions, FString& OutError);
FTakeRecorderActorSourceState CaptureActorSourceOptions(
    UTakeRecorderSource* Source,
    const TSharedPtr<FJsonObject>& Payload);
void RestoreActorSourceOptions(
    const TArray<FTakeRecorderActorSourceState>& States);
struct FTakeRecorderPanelSnapshot
{
    TWeakObjectPtr<ULevelSequence> Sequence;
    TWeakObjectPtr<UTakePreset> Preset;
    ETakeRecorderPanelMode Mode = ETakeRecorderPanelMode::NewRecording;
    FFrameRate FrameRate;
    bool bFrameRateFromTimecode = false;
    bool bValid = false;
};
struct FTakeRecorderStartRollbackState
{
    TWeakObjectPtr<UTakeRecorderPanel> Panel;
    TWeakObjectPtr<UTakeRecorderSources> Sources;
    FTakeRecorderPanelSnapshot PanelSnapshot;
    TArray<TStrongObjectPtr<UTakeRecorderSource>> SourceSnapshots;
    bool bRestoreSources = false;
    bool bRollbackAttempted = false;
    bool bRollbackSucceeded = false;
};
bool ConfigureSources(UTakeRecorderPanel* Panel, const TSharedPtr<FJsonObject>& Payload, int32& OutAdded, int32& OutRequested, FString& OutErrorCode, FString& OutError);
bool ConfigurePanel(UTakeRecorderPanel* Panel, const TSharedPtr<FJsonObject>& Payload, FString& OutErrorCode, FString& OutError);
bool CaptureTakeRecorderPanelSnapshot(
    UTakeRecorderPanel* Panel,
    FTakeRecorderPanelSnapshot& OutSnapshot,
    FString& OutError);
bool RestoreTakeRecorderPanelSnapshot(
    UTakeRecorderPanel* Panel,
    const FTakeRecorderPanelSnapshot& Snapshot);
TArray<TStrongObjectPtr<UTakeRecorderSource>> CaptureTakeRecorderSources(
    UTakeRecorderSources* Sources);
bool RestoreTakeRecorderSources(
    UTakeRecorderSources* Sources,
    const TArray<TStrongObjectPtr<UTakeRecorderSource>>& SourceSnapshots);
FString GetRecorderStateName(const UTakeRecorder* Recorder);
TSharedPtr<FJsonObject> MakeStartedRecordingResult(UTakeRecorder* Recorder);
void SendStartRecordingResult(
    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem,
    TWeakObjectPtr<UTakeRecorder> WeakRecorder,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    double DeadlineSeconds,
    TSharedRef<FTakeRecorderStartRollbackState> RollbackState);
TSharedPtr<FJsonObject> MakeStoppedRecordingResult(
    UTakeRecorder* Recorder,
    ULevelSequence* Sequence);
bool HandleCreateTakeRecorderPanel(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UTakeRecorderPanel* Panel);
bool HandleConfigureTakeSources(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UTakeRecorderPanel* Panel, bool& OutSucceeded);
bool HandleStartTakeRecording(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UTakeRecorderPanel* Panel, const FTakeRecorderPanelSnapshot& PanelSnapshot, bool& OutSucceeded);
bool HandleStopTakeRecording(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
bool HandleConfigureRecordedTracks(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UTakeRecorderPanel* Panel, bool& OutSucceeded);
#endif
}

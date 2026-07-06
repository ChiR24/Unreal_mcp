#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceTakeRecorderInternal.h"

#include "Engine/Engine.h"
#include "MovieScene.h"
#include "UObject/StrongObjectPtr.h"

namespace McpSequenceRecordReplay
{
#if MCP_SEQUENCE_HAS_TAKE_RECORDER_API
namespace
{
constexpr double TakeRecorderStartGraceSeconds = 10.0;

bool HasSourceConfigurationRequest(
    const TSharedPtr<FJsonObject>& Payload)
{
    if (!Payload.IsValid()) return false;
    static const TCHAR* Fields[] = {
        TEXT("actorName"),
        TEXT("actorNames"),
        TEXT("actors"),
        TEXT("sourceActors"),
        TEXT("sourceClasses"),
        TEXT("clearSources")
    };
    for (const TCHAR* Field : Fields)
        if (Payload->HasField(Field)) return true;
    return false;
}

void CountSources(UTakeRecorderSources* Sources, int32& OutSourceCount, int32& OutValidSourceCount)
{
    OutSourceCount = 0;
    OutValidSourceCount = 0;
    if (!Sources) return;
    for (UTakeRecorderSource* Source : Sources->GetSourcesCopy())
    {
        if (!Source) continue;
        ++OutSourceCount;
        if (Source->bEnabled && Source->IsValid()) ++OutValidSourceCount;
    }
}

}

bool RestoreTakeRecorderSources(
    UTakeRecorderSources* Sources,
    const TArray<TStrongObjectPtr<UTakeRecorderSource>>& SourceSnapshots)
{
    if (!Sources) return false;
    for (UTakeRecorderSource* Source : Sources->GetSourcesCopy())
        Sources->RemoveSource(Source);
    for (const TStrongObjectPtr<UTakeRecorderSource>& Snapshot :
         SourceSnapshots)
    {
        UTakeRecorderSource* Restored =
            Snapshot.IsValid()
                ? Sources->AddSource(Snapshot->GetClass())
                : nullptr;
        if (!Restored) return false;
        UEngine::CopyPropertiesForUnrelatedObjects(
            Snapshot.Get(), Restored);
    }
    return true;
}

TArray<TStrongObjectPtr<UTakeRecorderSource>> CaptureTakeRecorderSources(
    UTakeRecorderSources* Sources)
{
    TArray<TStrongObjectPtr<UTakeRecorderSource>> Snapshots;
    if (!Sources) return Snapshots;
    for (UTakeRecorderSource* Source : Sources->GetSourcesCopy())
    {
        UTakeRecorderSource* Snapshot =
            Source
                ? NewObject<UTakeRecorderSource>(
                      GetTransientPackage(), Source->GetClass())
                : nullptr;
        if (!Snapshot) continue;
        UEngine::CopyPropertiesForUnrelatedObjects(Source, Snapshot);
        Snapshots.Emplace(Snapshot);
    }
    return Snapshots;
}

bool HandleCreateTakeRecorderPanel(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UTakeRecorderPanel* Panel)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("panelOpen"), Panel->IsPanelOpen());
    Result->SetStringField(TEXT("mode"), StaticEnum<ETakeRecorderPanelMode>()->GetNameStringByValue(static_cast<int64>(Panel->GetMode())));
    if (ULevelSequence* Sequence = Panel->GetLevelSequence())
        Result->SetStringField(TEXT("sequencePath"), Sequence->GetPathName());
    const FFrameRate FrameRate = Panel->GetFrameRate();
    Result->SetNumberField(TEXT("frameRate"), FrameRate.AsDecimal());
    Result->SetNumberField(TEXT("frameRateNumerator"), FrameRate.Numerator);
    Result->SetNumberField(TEXT("frameRateDenominator"), FrameRate.Denominator);
    const UTakeRecorderSources* Sources = Panel->GetSources();
    Result->SetNumberField(
        TEXT("sourceCount"),
        Sources ? Sources->GetSourcesCopy().Num() : 0);
    if (const UTakeMetaData* MetaData = Panel->GetTakeMetaData())
        Result->SetBoolField(
            TEXT("frameRateFromTimecode"),
            MetaData->GetFrameRateFromTimecode());
    Subsystem->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Take Recorder panel is open"), Result);
    return true;
}

bool HandleStartTakeRecording(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UTakeRecorderPanel* Panel, const FTakeRecorderPanelSnapshot& PanelSnapshot, bool& OutSucceeded)
{
    OutSucceeded = false;
    if (UTakeRecorderBlueprintLibrary::GetActiveRecorder())
    {
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Take Recorder already has an active recording"), TEXT("ALREADY_RECORDING"));
        return true;
    }
    int32 AddedSources = 0;
    int32 RequestedSources = 0;
    FString SourceErrorCode;
    FString SourceError;
    UTakeRecorderSources* OriginalSourcesOwner =
        Panel ? Panel->GetSources() : nullptr;
    TArray<TStrongObjectPtr<UTakeRecorderSource>> OriginalSourceSnapshots =
        CaptureTakeRecorderSources(OriginalSourcesOwner);
    const bool bHasSourceConfiguration =
        HasSourceConfigurationRequest(Payload);
    if (bHasSourceConfiguration &&
        !ConfigureSources(
            Panel, Payload, AddedSources, RequestedSources,
            SourceErrorCode, SourceError))
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId, SourceError, SourceErrorCode);
        return true;
    }
    ULevelSequence* LevelSequence = Panel->GetLevelSequence();
    UTakeRecorderSources* Sources = LevelSequence ? LevelSequence->FindMetaData<UTakeRecorderSources>() : Panel->GetSources();
    int32 SourceCount = 0;
    int32 ValidSourceCount = 0;
    CountSources(Sources, SourceCount, ValidSourceCount);
    if (!LevelSequence || !Sources || ValidSourceCount == 0)
    {
        const bool bSourcesRestored =
            !bHasSourceConfiguration ||
            RestoreTakeRecorderSources(
                OriginalSourcesOwner, OriginalSourceSnapshots);
        if (!bSourcesRestored)
        {
            Subsystem->SendAutomationError(
                RequestingSocket, RequestId,
                TEXT("Take Recorder sources could not be restored after start failed"),
                TEXT("SOURCE_ROLLBACK_FAILED"));
            return true;
        }
        TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
        Details->SetBoolField(TEXT("hasSequence"), LevelSequence != nullptr);
        Details->SetNumberField(TEXT("sourceCount"), SourceCount);
        Details->SetNumberField(TEXT("validSourceCount"), ValidSourceCount);
        Details->SetNumberField(TEXT("requestedSources"), RequestedSources);
        Details->SetNumberField(TEXT("addedSources"), AddedSources);
        Details->SetBoolField(TEXT("sourcesRestored"), bSourcesRestored);
        Subsystem->SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Take Recorder has no valid enabled sources"), Details, TEXT("CANNOT_START_RECORDING"));
        return true;
    }
    FTakeRecorderParameters Parameters = UTakeRecorderBlueprintLibrary::GetDefaultParameters();
#if MCP_HAS_TAKE_RECORDER_OPEN_SEQUENCER
    Parameters.bOpenSequencer = false;
#endif
    Parameters.TakeRecorderMode = Panel->GetMode() == ETakeRecorderPanelMode::RecordingInto ? ETakeRecorderMode::RecordIntoSequence : ETakeRecorderMode::RecordNewSequence;
    Parameters.StartFrame = LevelSequence->GetMovieScene()->GetPlaybackRange().GetLowerBoundValue();
    UTakeRecorder* Recorder = UTakeRecorderBlueprintLibrary::StartRecording(LevelSequence, Sources, Panel->GetTakeMetaData(), Parameters);
    if (!Recorder)
    {
        if (bHasSourceConfiguration &&
            !RestoreTakeRecorderSources(
                OriginalSourcesOwner, OriginalSourceSnapshots))
        {
            Subsystem->SendAutomationError(
                RequestingSocket, RequestId,
                TEXT("Take Recorder sources could not be restored after start failed"),
                TEXT("SOURCE_ROLLBACK_FAILED"));
            return true;
        }
        Subsystem->SendAutomationError(RequestingSocket, RequestId, TEXT("Take Recorder did not create an active recorder"), TEXT("NOT_AVAILABLE"));
        return true;
    }
    const double DeadlineSeconds =
        FPlatformTime::Seconds() + Recorder->GetCountdownSeconds() +
        TakeRecorderStartGraceSeconds;
    TSharedRef<FTakeRecorderStartRollbackState> RollbackState =
        MakeShared<FTakeRecorderStartRollbackState>();
    RollbackState->Panel = Panel;
    RollbackState->Sources = OriginalSourcesOwner;
    RollbackState->PanelSnapshot = PanelSnapshot;
    RollbackState->SourceSnapshots =
        MoveTemp(OriginalSourceSnapshots);
    RollbackState->bRestoreSources = bHasSourceConfiguration;
    SendStartRecordingResult(
        TWeakObjectPtr<UMcpAutomationBridgeSubsystem>(Subsystem),
        TWeakObjectPtr<UTakeRecorder>(Recorder), RequestId, RequestingSocket,
        DeadlineSeconds, RollbackState);
    OutSucceeded = true;
    return true;
}

bool HandleStopTakeRecording(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    UTakeRecorder* Recorder = UTakeRecorderBlueprintLibrary::GetActiveRecorder();
    if (!Recorder || Recorder->GetState() != ETakeRecorderState::Started)
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId,
            Recorder
                ? FString::Printf(
                    TEXT("Take Recorder is not recording (state: %s)"),
                    *GetRecorderStateName(Recorder))
                : TEXT("Take Recorder is not recording"),
            TEXT("NOT_RECORDING"));
        return true;
    }
    ULevelSequence* Sequence = Recorder->GetSequence();
    UTakeRecorderBlueprintLibrary::StopRecording();
    TSharedPtr<FJsonObject> Result =
        MakeStoppedRecordingResult(Recorder, Sequence);
    const bool bHasRecordedData =
        Result->GetBoolField(TEXT("hasRecordedData"));
    Subsystem->SendAutomationResponse(
        RequestingSocket, RequestId, bHasRecordedData,
        bHasRecordedData
            ? TEXT("Take recording stopped with recorded sequence data")
            : TEXT("Take Recorder stopped without recorded sequence data"),
        Result,
        bHasRecordedData ? FString() : TEXT("RECORDING_OUTPUT_EMPTY"));
    return true;
}
#endif
}

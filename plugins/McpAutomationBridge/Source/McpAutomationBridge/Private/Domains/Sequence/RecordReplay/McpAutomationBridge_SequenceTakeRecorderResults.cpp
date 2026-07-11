#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceTakeRecorderInternal.h"

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Containers/Ticker.h"
#if MCP_HAS_MOVIE_SCENE_SHOT_METADATA
#include "MetaData/MovieSceneShotMetaData.h"
#endif
#include "MovieScene.h"
#include "UObject/Package.h"

namespace McpSequenceRecordReplay
{
#if MCP_SEQUENCE_HAS_TAKE_RECORDER_API
namespace
{
struct FTakeRecorderStartWaitState
{
    bool bCompleted = false;
    TWeakObjectPtr<UTakeRecorder> Recorder;
    TSharedPtr<FTakeRecorderStartRollbackState> RollbackState;
    FTSTicker::FDelegateHandle TickerHandle;
};

bool RollBackTakeRecorderStart(
    const TSharedPtr<FTakeRecorderStartRollbackState>& RollbackState)
{
    if (!RollbackState.IsValid()) return false;
    if (RollbackState->bRollbackAttempted)
        return RollbackState->bRollbackSucceeded;
    RollbackState->bRollbackAttempted = true;
    const bool bSourcesRestored =
        !RollbackState->bRestoreSources ||
        RestoreTakeRecorderSources(
            RollbackState->Sources.Get(),
            RollbackState->SourceSnapshots);
    const bool bPanelRestored =
        RestoreTakeRecorderPanelSnapshot(
            RollbackState->Panel.Get(),
            RollbackState->PanelSnapshot);
    RollbackState->bRollbackSucceeded =
        bSourcesRestored && bPanelRestored;
    return RollbackState->bRollbackSucceeded;
}

void CancelTakeRecorderStart(
    TSharedRef<FTakeRecorderStartWaitState> State)
{
    if (State->bCompleted) return;
    State->bCompleted = true;
    if (State->TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(State->TickerHandle);
        State->TickerHandle = FTSTicker::FDelegateHandle();
    }
    UTakeRecorder* Recorder = State->Recorder.Get();
    if (Recorder &&
        UTakeRecorderBlueprintLibrary::GetActiveRecorder() == Recorder)
    {
        UTakeRecorderBlueprintLibrary::CancelRecording();
    }
    RollBackTakeRecorderStart(State->RollbackState);
}

void SendTakeRecorderStartFailure(
    UMcpAutomationBridgeSubsystem* Subsystem,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    const FString& RequestId,
    const FString& Message,
    const FString& ErrorCode,
    TSharedRef<FTakeRecorderStartWaitState> State)
{
    const bool bRolledBack =
        RollBackTakeRecorderStart(State->RollbackState);
    TSharedPtr<FJsonObject> Result =
        McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("rolledBack"), bRolledBack);
    Subsystem->SendAutomationResponse(
        Socket, RequestId, false,
        bRolledBack
            ? Message
            : TEXT("Take Recorder start failed and configuration rollback failed"),
        Result,
        bRolledBack ? ErrorCode : TEXT("START_ROLLBACK_FAILED"));
}

void AddRecordingAssetPaths(
    const TSharedPtr<FJsonObject>& Result,
    const ULevelSequence* Sequence)
{
    const FString SequencePackagePath =
        Sequence && Sequence->GetOutermost()
            ? Sequence->GetOutermost()->GetName()
            : FString();
    Result->SetStringField(
        TEXT("sequencePackagePath"), SequencePackagePath);
    Result->SetStringField(
        TEXT("subsceneFolderPath"),
        SequencePackagePath.IsEmpty()
            ? FString()
            : SequencePackagePath + TEXT("_Subscenes"));
}
}

FString GetRecorderStateName(const UTakeRecorder* Recorder)
{
    return Recorder
        ? StaticEnum<ETakeRecorderState>()->GetNameStringByValue(
            static_cast<int64>(Recorder->GetState()))
        : TEXT("Unavailable");
}

TSharedPtr<FJsonObject> MakeStartedRecordingResult(UTakeRecorder* Recorder)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(
        TEXT("sequencePath"),
        Recorder && Recorder->GetSequence()
            ? Recorder->GetSequence()->GetPathName()
            : FString());
    AddRecordingAssetPaths(
        Result, Recorder ? Recorder->GetSequence() : nullptr);
    Result->SetStringField(TEXT("state"), GetRecorderStateName(Recorder));
    Result->SetBoolField(
        TEXT("recording"),
        Recorder && Recorder->GetState() == ETakeRecorderState::Started);
    Result->SetNumberField(
        TEXT("countdownSeconds"),
        Recorder ? Recorder->GetCountdownSeconds() : 0.0f);
    return Result;
}

void SendStartRecordingResult(
    TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem,
    TWeakObjectPtr<UTakeRecorder> WeakRecorder,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    double DeadlineSeconds,
    TSharedRef<FTakeRecorderStartRollbackState> RollbackState)
{
    UMcpAutomationBridgeSubsystem* Subsystem = WeakSubsystem.Get();
    if (!Subsystem) return;
    TSharedRef<FTakeRecorderStartWaitState> WaitState =
        MakeShared<FTakeRecorderStartWaitState>();
    WaitState->Recorder = WeakRecorder;
    WaitState->RollbackState = RollbackState;
    if (!Subsystem->RegisterAutomationRequestCancellation(
            RequestId,
            [WaitState]() { CancelTakeRecorderStart(WaitState); }))
    {
        CancelTakeRecorderStart(WaitState);
        return;
    }
    WaitState->TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateLambda(
            [WaitState, WeakSubsystem, WeakRecorder, RequestId, Socket,
             DeadlineSeconds](float)
            {
                if (WaitState->bCompleted) return false;
                UMcpAutomationBridgeSubsystem* Subsystem = WeakSubsystem.Get();
                UTakeRecorder* Recorder = WeakRecorder.Get();
                if (!Subsystem) return false;
                if (!Recorder)
                {
                    WaitState->bCompleted = true;
                    SendTakeRecorderStartFailure(
                        Subsystem,
                        Socket, RequestId,
                        TEXT("Take Recorder was released before recording started"),
                        TEXT("RECORDING_START_FAILED"), WaitState);
                    return false;
                }
                const ETakeRecorderState State = Recorder->GetState();
                if (State == ETakeRecorderState::Started)
                {
                    WaitState->bCompleted = true;
                    Subsystem->SendAutomationResponse(
                        Socket, RequestId, true, TEXT("Take recording started"),
                        MakeStartedRecordingResult(Recorder));
                    return false;
                }
                if (State == ETakeRecorderState::Stopped ||
                    State == ETakeRecorderState::Cancelled)
                {
                    WaitState->bCompleted = true;
                    SendTakeRecorderStartFailure(
                        Subsystem,
                        Socket, RequestId,
                        TEXT("Take Recorder stopped before recording began"),
                        TEXT("RECORDING_START_FAILED"), WaitState);
                    return false;
                }
                if (FPlatformTime::Seconds() >= DeadlineSeconds)
                {
                    WaitState->bCompleted = true;
                    if (UTakeRecorderBlueprintLibrary::GetActiveRecorder() == Recorder)
                        UTakeRecorderBlueprintLibrary::CancelRecording();
                    SendTakeRecorderStartFailure(
                        Subsystem,
                        Socket, RequestId,
                        TEXT("Timed out waiting for Take Recorder to start"),
                        TEXT("RECORDING_START_TIMEOUT"), WaitState);
                    return false;
                }
                return true;
            }),
        0.05f);
}

TSharedPtr<FJsonObject> MakeStoppedRecordingResult(
    UTakeRecorder* Recorder,
    ULevelSequence* Sequence)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(
        TEXT("sequencePath"), Sequence ? Sequence->GetPathName() : FString());
    AddRecordingAssetPaths(Result, Sequence);
    Result->SetStringField(TEXT("state"), GetRecorderStateName(Recorder));
    Result->SetBoolField(TEXT("recording"), false);
    bool bIsRecorded = false;
    int32 BindingCount = 0;
    int32 TrackCount = 0;
    if (Sequence)
    {
#if MCP_HAS_MOVIE_SCENE_SHOT_METADATA
        if (const UMovieSceneShotMetaData* MetaData =
                Sequence->FindMetaData<UMovieSceneShotMetaData>())
            bIsRecorded = MetaData->GetIsRecorded().Get(false);
#endif
        if (const UMovieScene* MovieScene = Sequence->GetMovieScene())
        {
            BindingCount = MovieScene->GetBindings().Num();
            TrackCount = MovieScene->GetTracks().Num();
            for (const FMovieSceneBinding& Binding : MovieScene->GetBindings())
                TrackCount += Binding.GetTracks().Num();
        }
#if !MCP_HAS_MOVIE_SCENE_SHOT_METADATA
        bIsRecorded = BindingCount > 0 || TrackCount > 0;
#endif
    }
    Result->SetBoolField(TEXT("isRecorded"), bIsRecorded);
    Result->SetNumberField(TEXT("bindingCount"), BindingCount);
    Result->SetNumberField(TEXT("trackCount"), TrackCount);
    Result->SetBoolField(
        TEXT("hasRecordedData"),
        bIsRecorded && (BindingCount > 0 || TrackCount > 0));
    return Result;
}
#endif
}

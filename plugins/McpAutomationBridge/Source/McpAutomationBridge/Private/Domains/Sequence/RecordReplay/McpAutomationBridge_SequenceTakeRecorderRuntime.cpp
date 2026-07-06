#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceTakeRecorderInternal.h"
#include "Domains/Sequence/McpAutomationBridge_SequenceFrameRate.h"

namespace McpSequenceRecordReplay
{
bool IsTakeRecorderAction(const FString& Action)
{
    return Action == TEXT("create_take_recorder_panel") ||
        Action == TEXT("configure_take_sources") ||
        Action == TEXT("start_recording") ||
        Action == TEXT("stop_recording") ||
        Action == TEXT("configure_recorded_tracks");
}

void SendTakeRecorderUnavailable(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    Subsystem->SendAutomationError(
        Socket, RequestId,
        TEXT("Take Recorder is unavailable. Enable the Takes plugin and compile "
             "the TakeRecorder, TakesCore, and TakeRecorderSources modules."),
        TEXT("NOT_AVAILABLE"));
}

#if MCP_SEQUENCE_HAS_TAKE_RECORDER_API
UTakeRecorderPanel* GetPanel(bool bOpen)
{
    if (!UTakeRecorderBlueprintLibrary::IsTakeRecorderEnabled()) return nullptr;
    return bOpen ? UTakeRecorderBlueprintLibrary::OpenTakeRecorderPanel() : UTakeRecorderBlueprintLibrary::GetTakeRecorderPanel();
}

AActor* FindTakeRecorderActor(const FString& Name)
{
    if (!GEditor || Name.IsEmpty()) return nullptr;
    TArray<UWorld*> Worlds;
    Worlds.Add(GEditor->GetEditorWorldContext().World());
    if (GEditor->PlayWorld) Worlds.AddUnique(GEditor->PlayWorld);

    for (UWorld* World : Worlds)
    {
        if (!World) continue;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetName().Equals(Name, ESearchCase::IgnoreCase) ||
                It->GetActorLabel().Equals(Name, ESearchCase::IgnoreCase) ||
                It->GetPathName().Equals(Name, ESearchCase::IgnoreCase))
            {
                return *It;
            }
        }
    }
    return nullptr;
}

namespace
{
struct FTakeRecorderPanelConfiguration
{
    TWeakObjectPtr<UTakeRecorderPanel> Panel;
    TWeakObjectPtr<ULevelSequence> RequestedSequence;
    ETakeRecorderPanelMode Mode = ETakeRecorderPanelMode::NewRecording;
    bool bConfigured = false;

    bool Matches(
        UTakeRecorderPanel* CandidatePanel,
        ULevelSequence* CandidateSequence,
        ETakeRecorderPanelMode CandidateMode) const
    {
        return bConfigured &&
            Panel.Get() == CandidatePanel &&
            RequestedSequence.Get() == CandidateSequence &&
            Mode == CandidateMode;
    }

    void Set(
        UTakeRecorderPanel* InPanel,
        ULevelSequence* InRequestedSequence,
        ETakeRecorderPanelMode InMode)
    {
        Panel = InPanel;
        RequestedSequence = InRequestedSequence;
        Mode = InMode;
        bConfigured = true;
    }

    void Reset()
    {
        Panel.Reset();
        RequestedSequence.Reset();
        Mode = ETakeRecorderPanelMode::NewRecording;
        bConfigured = false;
    }
};

FTakeRecorderPanelConfiguration ActivePanelConfiguration;

FString GetRequestedSequencePath(const TSharedPtr<FJsonObject>& Payload)
{
    FString Path = McpHandlerUtils::GetOptionalString(Payload, TEXT("sequencePath"));
    if (Path.IsEmpty()) Path = McpHandlerUtils::GetOptionalString(Payload, TEXT("path"));
    if (Path.IsEmpty()) Path = McpHandlerUtils::GetOptionalString(Payload, TEXT("recordingSequencePath"));
    if (Path.IsEmpty()) Path = McpHandlerUtils::GetOptionalString(Payload, TEXT("takeSequencePath"));
    return Path;
}
}

bool ConfigurePanel(
    UTakeRecorderPanel* Panel,
    const TSharedPtr<FJsonObject>& Payload,
    FString& OutErrorCode,
    FString& OutError)
{
    if (!Panel)
    {
        OutErrorCode = TEXT("NOT_AVAILABLE");
        OutError = TEXT("Take Recorder panel is not open");
        return false;
    }
    const bool bHasFrameRate =
        Payload.IsValid() && Payload->HasField(TEXT("frameRate"));
    FFrameRate FrameRate;
    if (bHasFrameRate &&
        !McpSequenceFrameRate::TryParse(
            Payload, TEXT("frameRate"), FrameRate, OutError))
    {
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }
    bool bRecordInto = false;
    if (Payload.IsValid() && Payload->HasField(TEXT("recordInto")) &&
        !Payload->TryGetBoolField(TEXT("recordInto"), bRecordInto))
    {
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        OutError = TEXT("recordInto must be a boolean");
        return false;
    }
    const FString SequencePath = GetRequestedSequencePath(Payload);
    if (!SequencePath.IsEmpty())
    {
        ULevelSequence* Sequence =
            Cast<ULevelSequence>(UEditorAssetLibrary::LoadAsset(SequencePath));
        if (!Sequence)
        {
            OutErrorCode = TEXT("INVALID_SEQUENCE");
            OutError = FString::Printf(
                TEXT("Take Recorder sequence is invalid: %s"), *SequencePath);
            return false;
        }
        const ETakeRecorderPanelMode RequestedMode =
            bRecordInto
                ? ETakeRecorderPanelMode::RecordingInto
                : ETakeRecorderPanelMode::NewRecording;
        if (!ActivePanelConfiguration.Matches(
                Panel, Sequence, RequestedMode) ||
            Panel->GetMode() != RequestedMode)
        {
            if (bRecordInto)
                Panel->SetupForRecordingInto_LevelSequence(Sequence);
            else
                Panel->SetupForRecording_LevelSequence(Sequence);
            ActivePanelConfiguration.Set(Panel, Sequence, RequestedMode);
        }
    }
    if (bHasFrameRate)
    {
        Panel->SetFrameRate(FrameRate);
        if (UTakeMetaData* MetaData = Panel->GetTakeMetaData())
        {
            MetaData->SetFrameRateFromTimecode(false);
        }
    }
    return true;
}

bool RestoreTakeRecorderPanelSnapshot(
    UTakeRecorderPanel* Panel,
    const FTakeRecorderPanelSnapshot& Snapshot)
{
    if (!Panel || !Snapshot.bValid)
    {
        return false;
    }

    ULevelSequence* Sequence = Snapshot.Sequence.Get();
    switch (Snapshot.Mode)
    {
    case ETakeRecorderPanelMode::NewRecording:
        if (Sequence)
            Panel->SetupForRecording_LevelSequence(Sequence);
        else
            Panel->ClearPendingTake();
        break;
    case ETakeRecorderPanelMode::RecordingInto:
        if (!Sequence) return false;
        Panel->SetupForRecordingInto_LevelSequence(Sequence);
        break;
    case ETakeRecorderPanelMode::EditingPreset:
        if (!Snapshot.Preset.IsValid()) return false;
        Panel->SetupForEditing(Snapshot.Preset.Get());
        break;
    case ETakeRecorderPanelMode::ReviewingRecording:
        if (!Sequence) return false;
        Panel->SetupForViewing(Sequence);
        break;
    default:
        return false;
    }

    Panel->SetFrameRate(Snapshot.FrameRate);
    if (UTakeMetaData* MetaData = Panel->GetTakeMetaData())
    {
        MetaData->SetFrameRateFromTimecode(
            Snapshot.bFrameRateFromTimecode);
    }
    if (Sequence &&
        (Snapshot.Mode == ETakeRecorderPanelMode::NewRecording ||
         Snapshot.Mode == ETakeRecorderPanelMode::RecordingInto))
    {
        ActivePanelConfiguration.Set(
            Panel, Sequence, Snapshot.Mode);
    }
    else
    {
        ActivePanelConfiguration.Reset();
    }
    return true;
}
#endif
}

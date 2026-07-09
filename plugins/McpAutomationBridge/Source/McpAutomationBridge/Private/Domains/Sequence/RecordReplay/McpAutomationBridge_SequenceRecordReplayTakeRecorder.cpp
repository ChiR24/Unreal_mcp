#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceTakeRecorderInternal.h"

namespace McpSequenceRecordReplay
{
bool HandleTakeRecorderAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (!IsTakeRecorderAction(Action))
    {
        return false;
    }

#if !MCP_SEQUENCE_HAS_TAKE_RECORDER_API
    SendTakeRecorderUnavailable(Subsystem, RequestId, RequestingSocket);
    return true;
#else
    if (Action == TEXT("stop_recording"))
    {
        return HandleStopTakeRecording(
            Subsystem, RequestId, RequestingSocket);
    }

    UTakeRecorderPanel* Panel = GetPanel(true);
    if (!Panel)
    {
        SendTakeRecorderUnavailable(Subsystem, RequestId, RequestingSocket);
        return true;
    }

    if (Action == TEXT("create_take_recorder_panel"))
    {
        return HandleCreateTakeRecorderPanel(
            Subsystem, RequestId, RequestingSocket, Panel);
    }
    if (UTakeRecorderBlueprintLibrary::GetActiveRecorder())
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId,
            TEXT("Take Recorder is already recording"),
            TEXT("RECORDING_ACTIVE"));
        return true;
    }

    FTakeRecorderPanelSnapshot Snapshot;
    FString SnapshotError;
    if (!CaptureTakeRecorderPanelSnapshot(Panel, Snapshot, SnapshotError))
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId, SnapshotError,
            TEXT("PANEL_SNAPSHOT_FAILED"));
        return true;
    }

    FString PanelErrorCode;
    FString PanelError;
    if (!ConfigurePanel(Panel, Payload, PanelErrorCode, PanelError))
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId, PanelError, PanelErrorCode);
        return true;
    }

    bool bSucceeded = false;
    bool bHandled = false;
    if (Action == TEXT("configure_take_sources"))
    {
        bHandled = HandleConfigureTakeSources(
            Subsystem, RequestId, Payload, RequestingSocket, Panel,
            bSucceeded);
    }
    else if (Action == TEXT("start_recording"))
    {
        bHandled = HandleStartTakeRecording(
            Subsystem, RequestId, Payload, RequestingSocket, Panel,
            Snapshot, bSucceeded);
    }
    else
    {
        bHandled = HandleConfigureRecordedTracks(
            Subsystem, RequestId, Payload, RequestingSocket, Panel,
            bSucceeded);
    }
    if (!bSucceeded)
    {
        RestoreTakeRecorderPanelSnapshot(Panel, Snapshot);
    }
    return bHandled;
#endif
}
}

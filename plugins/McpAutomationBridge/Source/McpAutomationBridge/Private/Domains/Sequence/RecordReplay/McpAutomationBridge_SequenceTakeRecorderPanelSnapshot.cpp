#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceTakeRecorderInternal.h"

namespace McpSequenceRecordReplay
{
#if MCP_SEQUENCE_HAS_TAKE_RECORDER_API
bool CaptureTakeRecorderPanelSnapshot(
    UTakeRecorderPanel* Panel,
    FTakeRecorderPanelSnapshot& OutSnapshot,
    FString& OutError)
{
    OutSnapshot = FTakeRecorderPanelSnapshot();
    if (!Panel || !Panel->IsPanelOpen())
    {
        OutError = TEXT("Take Recorder panel is not open");
        return false;
    }

    OutSnapshot.Sequence = Panel->GetLevelSequence();
    OutSnapshot.Mode = Panel->GetMode();
    OutSnapshot.FrameRate = Panel->GetFrameRate();
    if (UTakeMetaData* MetaData = Panel->GetTakeMetaData())
    {
        OutSnapshot.Preset = MetaData->GetPresetOrigin();
        OutSnapshot.bFrameRateFromTimecode =
            MetaData->GetFrameRateFromTimecode();
    }
    OutSnapshot.bValid = true;
    return true;
}
#endif
}

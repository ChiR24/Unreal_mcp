#include "Domains/Sequence/RecordReplay/McpSeqRecordReplay.h"

namespace McpSequenceRecordReplay
{
bool TryHandle(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // The sequence dispatcher passes a lower-case action.
    FString NormalizedAction = Action;
    NormalizedAction.RemoveFromStart(TEXT("sequence_"));

    if (HandleTakeRecorderAction(
            Subsystem, RequestId, NormalizedAction, Payload, RequestingSocket))
    {
        return true;
    }

    return HandleDemoReplayAction(
        Subsystem, RequestId, NormalizedAction, Payload, RequestingSocket);
}
}

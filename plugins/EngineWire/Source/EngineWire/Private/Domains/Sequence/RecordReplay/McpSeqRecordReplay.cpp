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
    if (!Subsystem)
    {
        return false;
    }

    FString NormalizedAction = Action.ToLower();
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

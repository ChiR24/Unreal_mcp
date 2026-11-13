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
    if (HandleTakeRecorderAction(
            Subsystem, RequestId, Action, Payload, RequestingSocket))
    {
        return true;
    }

    return HandleDemoReplayAction(
        Subsystem, RequestId, Action, Payload, RequestingSocket);
}
}

#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceTakeRecorderInternal.h"

namespace McpSequenceRecordReplay
{
#if MCP_SEQUENCE_HAS_TAKE_RECORDER_API
bool HandleConfigureTakeSources(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UTakeRecorderPanel* Panel,
    bool& OutSucceeded)
{
    OutSucceeded = false;
    int32 Added = 0;
    int32 Requested = 0;
    FString ErrorCode;
    FString Error;
    if (!ConfigureSources(
            Panel, Payload, Added, Requested, ErrorCode, Error))
    {
        Subsystem->SendAutomationError(
            RequestingSocket, RequestId, Error, ErrorCode);
        return true;
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("addedSources"), Added);
    Result->SetNumberField(TEXT("requestedSources"), Requested);
    Result->SetNumberField(
        TEXT("sourceCount"), Panel->GetSources()->GetSourcesCopy().Num());
    Subsystem->SendAutomationResponse(
        RequestingSocket, RequestId, true,
        TEXT("Take Recorder sources configured"), Result);
    OutSucceeded = true;
    return true;
}
#endif
}

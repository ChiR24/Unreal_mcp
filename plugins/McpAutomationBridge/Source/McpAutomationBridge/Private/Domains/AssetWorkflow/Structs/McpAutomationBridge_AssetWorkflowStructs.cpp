#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

#if WITH_EDITOR

bool UMcpAutomationBridgeSubsystem::HandleStructAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (HandleStructLifecycleActions(*this, RequestId, Action, Payload, RequestingSocket))
    {
        return true;
    }
    if (HandleStructMemberAddRemoveActions(*this, RequestId, Action, Payload, RequestingSocket))
    {
        return true;
    }
    if (HandleStructMemberEditActions(*this, RequestId, Action, Payload, RequestingSocket))
    {
        return true;
    }
    if (HandleStructAnalysisActions(*this, RequestId, Action, Payload, RequestingSocket))
    {
        return true;
    }
    if (HandleStructSerializationActions(*this, RequestId, Action, Payload, RequestingSocket))
    {
        return true;
    }
    if (HandleStructImportActions(*this, RequestId, Action, Payload, RequestingSocket))
    {
        return true;
    }
    if (HandleStructAssetActions(*this, RequestId, Action, Payload, RequestingSocket))
    {
        return true;
    }
    return false;
}

#endif // WITH_EDITOR

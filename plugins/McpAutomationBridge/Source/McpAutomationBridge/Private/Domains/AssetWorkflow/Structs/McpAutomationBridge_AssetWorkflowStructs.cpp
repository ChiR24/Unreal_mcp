#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"


UUserDefinedStruct* LoadStructOrReply(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket, const FString& StructPath)
{
    if (StructPath.IsEmpty())
    {
        Bridge.SendAutomationError(Socket, RequestId, TEXT("Missing required parameter: structPath"), TEXT("MISSING_PARAMETER"));
        return nullptr;
    }
    UUserDefinedStruct* S = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
    if (!S)
    {
        Bridge.SendAutomationError(Socket, RequestId, FString::Printf(TEXT("Struct not found: %s"), *StructPath), TEXT("ASSET_NOT_FOUND"));
    }
    return S;
}

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


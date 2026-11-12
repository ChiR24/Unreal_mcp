#include "Domains/AssetWorkflow/Structs/McpAssetStructsShared.h"


UUserDefinedStruct* LoadStructOrReply(UEngineWireSubsystem& Bridge, const FString& RequestId,
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

bool UEngineWireSubsystem::HandleStructAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Each group tests the name itself and returns false, sending nothing, for one it does not own.
    return HandleStructLifecycleActions(*this, RequestId, Action, Payload, RequestingSocket)
        || HandleStructMemberAddRemoveActions(*this, RequestId, Action, Payload, RequestingSocket)
        || HandleStructMemberEditActions(*this, RequestId, Action, Payload, RequestingSocket)
        || HandleStructAnalysisActions(*this, RequestId, Action, Payload, RequestingSocket)
        || HandleStructSerializationActions(*this, RequestId, Action, Payload, RequestingSocket)
        || HandleStructImportActions(*this, RequestId, Action, Payload, RequestingSocket)
        || HandleStructAssetAction_Delete(*this, RequestId, Action, Payload, RequestingSocket)
        || HandleStructAssetAction_Rename(*this, RequestId, Action, Payload, RequestingSocket)
        || HandleStructAssetAction_Refresh(*this, RequestId, Action, Payload, RequestingSocket);
}


#include "McpAutomationBridge_AssetQueryHandlersPrivate.h"

bool UMcpAutomationBridgeSubsystem::HandleAssetQueryAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (!Action.Equals(TEXT("asset_query"), ESearchCase::IgnoreCase))
    {
        return false;
    }

    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));
    using namespace McpAssetQueryHandlers;

    if (SubAction == TEXT("get_dependencies"))
    {
        return McpAssetQueryHandlers::HandleGetDependencies(
            this, RequestId, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("find_by_tag"))
    {
        return HandleFindByMetadataTag(this, RequestId, Payload, RequestingSocket);
    }
    if (SubAction == TEXT("search_assets"))
    {
        return McpAssetQueryHandlers::HandleSearchAssets(
            this, RequestId, Payload, RequestingSocket);
    }
#if WITH_EDITOR
    if (SubAction == TEXT("get_source_control_state"))
    {
        return McpAssetQueryHandlers::HandleGetSourceControlState(
            this, RequestId, Payload, RequestingSocket);
    }
#endif

    SendAutomationError(RequestingSocket, RequestId,
        TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
    return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSearchAssets(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> RoutedPayload = Payload;
    if (Payload.IsValid() && !Payload->HasField(TEXT("subAction")))
    {
        RoutedPayload = MakeShared<FJsonObject>(*Payload);
        RoutedPayload->SetStringField(TEXT("subAction"), TEXT("search_assets"));
    }

    return HandleAssetQueryAction(RequestId, TEXT("asset_query"), RoutedPayload, RequestingSocket);
}

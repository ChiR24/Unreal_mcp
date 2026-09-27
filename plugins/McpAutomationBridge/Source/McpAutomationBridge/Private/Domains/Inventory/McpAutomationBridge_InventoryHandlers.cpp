#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpAutomationBridge_InventoryHandlersShared.h"

bool UMcpAutomationBridgeSubsystem::HandleManageInventoryAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (Action != TEXT("manage_inventory")) {
    return false;
  }

  const FString SubAction = GetJsonStringField(Payload, TEXT("subAction"));

  if (HandleInventoryDataAssetActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryCategoryActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryReplicationActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryLootTableActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryLootDropActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryCraftingRecipeActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryCraftingStationActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryItemPresentationActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryInfoActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  SendAutomationError(
      RequestingSocket, RequestId,
      FString::Printf(TEXT("Unknown inventory action: %s"), *SubAction),
      TEXT("UNKNOWN_ACTION"));
  return true;
}

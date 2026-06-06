#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_InventoryHandlersShared.h"

bool UMcpAutomationBridgeSubsystem::HandleManageInventoryAction(
    const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (Action != TEXT("manage_inventory")) {
    return false;
  }

  const FString SubAction = GetPayloadString(Payload, TEXT("subAction"));

  if (HandleInventoryDataAssetActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryCategoryActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryComponentActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryFunctionActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryReplicationActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryPickupActorActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryPickupBehaviorActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryEquipmentComponentActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryEquipmentEffectActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryEquipmentFunctionActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
    return true;
  }

  if (HandleInventoryEquipmentVisualActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
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

  if (HandleInventoryCraftingComponentActions(*this, RequestId, SubAction, Payload, RequestingSocket)) {
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

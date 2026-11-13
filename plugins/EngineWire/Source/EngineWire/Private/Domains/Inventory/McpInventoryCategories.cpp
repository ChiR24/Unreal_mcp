#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpInventoryShared.h"

bool HandleInventoryCategoryActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("create_item_category")) {
    return CreateGenericInventoryAsset(Bridge, RequestId, RequestingSocket, Payload, TEXT("/Game/Items/Categories"), TEXT("categoryPath"),
                                       TEXT("Item category created"), TEXT("category"));
  }

  if (SubAction == TEXT("assign_item_category")) {
    FString ItemPath = GetJsonStringField(Payload, TEXT("itemPath"));
    FString CategoryPath = GetJsonStringField(Payload, TEXT("categoryPath"));

    if (ItemPath.IsEmpty() || CategoryPath.IsEmpty()) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Missing required parameters: itemPath and categoryPath"),
          TEXT("MISSING_PARAMETER"));
      return true;
    }

    UObject* ItemObj = LoadInventoryAssetOrError(
        Bridge, RequestId, RequestingSocket, ItemPath, TEXT("itemPath"), TEXT("Item"));
    if (!ItemObj) {
      return true;
    }
    // The category path used to be stored without a look, so a typo saved a category that does not exist.
    UObject* CategoryObj = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *CategoryPath);
    if (!CategoryObj) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
          FString::Printf(TEXT("Category not found: %s (create it with create_inventory_asset kind=item_category); nothing was changed."), *CategoryPath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    bool bCategoryAssigned = false;
    FString AssignError;

    // Try to find a "Category" property on the item and set it via reflection
    FProperty* CategoryProp = ItemObj->GetClass()->FindPropertyByName(TEXT("Category"));
    if (!CategoryProp) {
      CategoryProp = ItemObj->GetClass()->FindPropertyByName(TEXT("ItemCategory"));
    }

    if (CategoryProp) {
      TSharedPtr<FJsonValue> CategoryValue = MakeShared<FJsonValueString>(CategoryPath);
      if (ApplyJsonValueToProperty(ItemObj, CategoryProp, CategoryValue, AssignError)) {
        bCategoryAssigned = true;
      }
    } else {
      // Try to find a soft object reference property for category
      for (TFieldIterator<FProperty> It(ItemObj->GetClass()); It; ++It) {
        FProperty* Prop = *It;
        if (Prop->GetName().Contains(TEXT("Category"), ESearchCase::IgnoreCase)) {
          TSharedPtr<FJsonValue> CategoryValue = MakeShared<FJsonValueString>(CategoryPath);
          if (ApplyJsonValueToProperty(ItemObj, Prop, CategoryValue, AssignError)) {
            bCategoryAssigned = true;
            break;
          }
        }
      }
    }

    if (!bCategoryAssigned) {
      // Generic items carry their category in the Properties map (read back by
      // get_inventory_info); reporting success without storing it was a no-op.
      if (UMcpGenericDataAsset* GenericItem = Cast<UMcpGenericDataAsset>(ItemObj)) {
        GenericItem->Properties.Add(TEXT("Category"), CategoryPath);
        bCategoryAssigned = true;
      }
    }
    if (!bCategoryAssigned) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Item class has no Category/ItemCategory property to assign"),
                          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
    ItemObj->MarkPackageDirty();

    const bool bSaved = GetJsonBoolField(Payload, TEXT("save"), true) && McpSafeAssetSave(ItemObj);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    Result->SetStringField(TEXT("categoryPath"), CategoryPath);
    Result->SetBoolField(TEXT("assigned"), bCategoryAssigned);
    McpHandlerUtils::AddVerification(Result, ItemObj);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Category assigned to item"), Result);
    return true;
  }

  return false;
}

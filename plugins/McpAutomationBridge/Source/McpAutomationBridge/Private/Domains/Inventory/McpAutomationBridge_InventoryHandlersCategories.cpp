#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryCategoryActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("create_item_category")) {
    FString Name = GetPayloadString(Payload, TEXT("name"));
    FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Items/Categories"));

    if (Name.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: name"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    UPackage* Package = CreateInventoryAssetPackage(Path, Name);
    if (!Package) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create package"),
                          TEXT("PACKAGE_CREATE_FAILED"));
      return true;
    }

    // UMcpGenericDataAsset (UDataAsset/UPrimaryDataAsset are abstract in UE5)
    UMcpGenericDataAsset* CategoryAsset =
        NewObject<UMcpGenericDataAsset>(Package, FName(*Name), RF_Public | RF_Standalone);

    if (CategoryAsset) {
      CategoryAsset->MarkPackageDirty();
      FAssetRegistryModule::AssetCreated(CategoryAsset);

      if (GetPayloadBool(Payload, TEXT("save"), true)) {
        McpSafeAssetSave(CategoryAsset);
      }

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("categoryPath"), Package->GetName());
      Result->SetStringField(TEXT("assetPath"), Package->GetName() + TEXT(".") + FPackageName::GetShortName(Package->GetName())); // dogfood #55: consistent object path
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Item category created"), Result);
    } else {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create category asset"),
                          TEXT("ASSET_CREATE_FAILED"));
    }
    return true;
  }

  if (SubAction == TEXT("assign_item_category")) {
    FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));
    FString CategoryPath = GetPayloadString(Payload, TEXT("categoryPath"));

    if (ItemPath.IsEmpty() || CategoryPath.IsEmpty()) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Missing required parameters: itemPath and categoryPath"),
          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load both assets (use UDataAsset base class for loading - UPrimaryDataAsset is abstract in UE5.7)
    UObject* ItemObj = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
    UObject* CategoryObj = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *CategoryPath);

    if (!ItemObj) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Item not found: %s"), *ItemPath),
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

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(ItemObj);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    Result->SetStringField(TEXT("categoryPath"), CategoryPath);
    Result->SetBoolField(TEXT("assigned"), bCategoryAssigned);
    if (!bCategoryAssigned && !AssignError.IsEmpty()) {
      Result->SetStringField(TEXT("note"), TEXT("Category property not found on item class. Ensure your item class has a Category or ItemCategory property."));
    }
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Category assigned to item"), Result);
    return true;
  }

  // ===========================================================================
  // 17.2 Inventory Component (5 actions)
  // ===========================================================================

  return false;
}

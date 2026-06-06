#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryItemPresentationActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("configure_item_stacking")) {
    FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));

    if (ItemPath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: itemPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the item asset
    UObject* ItemAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
    if (!ItemAsset) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Item not found: %s"), *ItemPath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    bool bStackable = GetPayloadBool(Payload, TEXT("stackable"), true);
    int32 MaxStackSize = static_cast<int32>(GetPayloadNumber(Payload, TEXT("maxStackSize"), 99));
    bool bUniqueItems = GetPayloadBool(Payload, TEXT("uniqueItems"), false);

    TArray<FString> ModifiedProps;

    // Try to set stacking properties via reflection
    FProperty* StackableProp = ItemAsset->GetClass()->FindPropertyByName(TEXT("bStackable"));
    if (!StackableProp) {
      StackableProp = ItemAsset->GetClass()->FindPropertyByName(TEXT("Stackable"));
    }
    if (StackableProp) {
      TSharedPtr<FJsonValue> BoolVal = MakeShared<FJsonValueBoolean>(bStackable);
      FString ApplyError;
      if (ApplyJsonValueToProperty(ItemAsset, StackableProp, BoolVal, ApplyError)) {
        ModifiedProps.Add(TEXT("Stackable"));
      }
    }

    FProperty* MaxStackProp = ItemAsset->GetClass()->FindPropertyByName(TEXT("MaxStackSize"));
    if (!MaxStackProp) {
      MaxStackProp = ItemAsset->GetClass()->FindPropertyByName(TEXT("StackLimit"));
    }
    if (MaxStackProp) {
      TSharedPtr<FJsonValue> IntVal = MakeShared<FJsonValueNumber>(static_cast<double>(MaxStackSize));
      FString ApplyError;
      if (ApplyJsonValueToProperty(ItemAsset, MaxStackProp, IntVal, ApplyError)) {
        ModifiedProps.Add(TEXT("MaxStackSize"));
      }
    }

    FProperty* UniqueProp = ItemAsset->GetClass()->FindPropertyByName(TEXT("bUniqueItem"));
    if (UniqueProp) {
      TSharedPtr<FJsonValue> BoolVal = MakeShared<FJsonValueBoolean>(bUniqueItems);
      FString ApplyError;
      if (ApplyJsonValueToProperty(ItemAsset, UniqueProp, BoolVal, ApplyError)) {
        ModifiedProps.Add(TEXT("UniqueItem"));
      }
    }

    if (ModifiedProps.Num() == 0) {
      if (UMcpGenericDataAsset* GenericItem = Cast<UMcpGenericDataAsset>(ItemAsset)) {
        GenericItem->Properties.Add(TEXT("bStackable"), bStackable ? TEXT("true") : TEXT("false"));
        GenericItem->Properties.Add(TEXT("MaxStackSize"), FString::FromInt(MaxStackSize));
        GenericItem->Properties.Add(TEXT("bUniqueItem"), bUniqueItems ? TEXT("true") : TEXT("false"));
        ModifiedProps.Add(TEXT("Properties.bStackable"));
        ModifiedProps.Add(TEXT("Properties.MaxStackSize"));
        ModifiedProps.Add(TEXT("Properties.bUniqueItem"));
      }
    }

    ItemAsset->MarkPackageDirty();

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(ItemAsset);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    Result->SetBoolField(TEXT("stackable"), bStackable);
    Result->SetNumberField(TEXT("maxStackSize"), MaxStackSize);
    Result->SetBoolField(TEXT("uniqueItems"), bUniqueItems);

    TArray<TSharedPtr<FJsonValue>> ModArr;
    for (const FString& Prop : ModifiedProps) {
      ModArr.Add(MakeShared<FJsonValueString>(Prop));
    }
    Result->SetArrayField(TEXT("modifiedProperties"), ModArr);
    Result->SetBoolField(TEXT("configured"), ModifiedProps.Num() > 0);

    if (ModifiedProps.Num() == 0) {
      Result->SetStringField(TEXT("note"), TEXT("No stacking properties found. Ensure your item class has bStackable, MaxStackSize, or StackLimit properties."));
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Item stacking configured"), Result);
    return true;
  }

  if (SubAction == TEXT("set_item_icon")) {
    FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));
    FString IconPath = GetPayloadString(Payload, TEXT("iconPath"));

    if (ItemPath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: itemPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the item asset
    UObject* ItemAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
    if (!ItemAsset) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Item not found: %s"), *ItemPath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    bool bIconSet = false;
    FString IconPropertyName;

    // Try common icon property names
    TArray<FString> IconPropNames = {
      TEXT("Icon"),
      TEXT("ItemIcon"),
      TEXT("Thumbnail"),
      TEXT("DisplayIcon"),
      TEXT("InventoryIcon")
    };

    for (const FString& PropName : IconPropNames) {
      FProperty* IconProp = ItemAsset->GetClass()->FindPropertyByName(*PropName);
      if (IconProp) {
        TSharedPtr<FJsonValue> PathVal = MakeShared<FJsonValueString>(IconPath);
        FString ApplyError;
        if (ApplyJsonValueToProperty(ItemAsset, IconProp, PathVal, ApplyError)) {
          bIconSet = true;
          IconPropertyName = PropName;
          break;
        }
      }
    }

    if (!bIconSet) {
      if (UMcpGenericDataAsset* GenericItem = Cast<UMcpGenericDataAsset>(ItemAsset)) {
        GenericItem->Properties.Add(TEXT("IconPath"), IconPath);
        bIconSet = true;
        IconPropertyName = TEXT("Properties.IconPath");
      }
    }

    ItemAsset->MarkPackageDirty();

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(ItemAsset);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    Result->SetStringField(TEXT("iconPath"), IconPath);
    Result->SetBoolField(TEXT("iconSet"), bIconSet);
    if (bIconSet) {
      Result->SetStringField(TEXT("propertyModified"), IconPropertyName);
    } else {
      Result->SetStringField(TEXT("note"), TEXT("No icon property found. Ensure your item class has an Icon, ItemIcon, or Thumbnail property."));
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Item icon configured"), Result);
    return true;
  }

  return false;
}

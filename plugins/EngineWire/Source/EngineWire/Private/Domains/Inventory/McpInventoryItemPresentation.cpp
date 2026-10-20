#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpInventoryShared.h"

bool HandleInventoryItemPresentationActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("configure_item_stacking")) {
    FString ItemPath = GetJsonStringField(Payload, TEXT("itemPath"));

    if (ItemPath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: itemPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    UObject* ItemAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
    if (!ItemAsset) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Item not found: %s"), *ItemPath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Only the fields sent are written: an omitted one used to be reset to stackable, 99 or not unique.
    const bool bHasStackable = Payload->HasField(TEXT("stackable"));
    const bool bHasMax = Payload->HasField(TEXT("maxStackSize"));
    const bool bHasUnique = Payload->HasField(TEXT("uniqueItems"));
    const bool bStackable = GetJsonBoolField(Payload, TEXT("stackable"), true);
    const int32 MaxStackSize = static_cast<int32>(GetJsonNumberField(Payload, TEXT("maxStackSize"), 99));
    const bool bUniqueItems = GetJsonBoolField(Payload, TEXT("uniqueItems"), false);
    if ((!bHasStackable && !bHasMax && !bHasUnique) || (bHasMax && MaxStackSize < 1)) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
          TEXT("Send stackable, maxStackSize (1 or more) or uniqueItems; nothing was changed."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    TArray<FString> ModifiedProps;

    FProperty* StackableProp = ItemAsset->GetClass()->FindPropertyByName(TEXT("bStackable"));
    if (!StackableProp) {
      StackableProp = ItemAsset->GetClass()->FindPropertyByName(TEXT("Stackable"));
    }
    if (StackableProp && bHasStackable) {
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
    if (MaxStackProp && bHasMax) {
      TSharedPtr<FJsonValue> IntVal = MakeShared<FJsonValueNumber>(static_cast<double>(MaxStackSize));
      FString ApplyError;
      if (ApplyJsonValueToProperty(ItemAsset, MaxStackProp, IntVal, ApplyError)) {
        ModifiedProps.Add(TEXT("MaxStackSize"));
      }
    }

    FProperty* UniqueProp = ItemAsset->GetClass()->FindPropertyByName(TEXT("bUniqueItem"));
    if (UniqueProp && bHasUnique) {
      TSharedPtr<FJsonValue> BoolVal = MakeShared<FJsonValueBoolean>(bUniqueItems);
      FString ApplyError;
      if (ApplyJsonValueToProperty(ItemAsset, UniqueProp, BoolVal, ApplyError)) {
        ModifiedProps.Add(TEXT("UniqueItem"));
      }
    }

    if (ModifiedProps.Num() == 0) {
      if (UMcpGenericDataAsset* GenericItem = Cast<UMcpGenericDataAsset>(ItemAsset)) {
        if (bHasStackable) {
          GenericItem->Properties.Add(TEXT("bStackable"), bStackable ? TEXT("true") : TEXT("false"));
          ModifiedProps.Add(TEXT("Properties.bStackable"));
        }
        if (bHasMax) {
          GenericItem->Properties.Add(TEXT("MaxStackSize"), FString::FromInt(MaxStackSize));
          ModifiedProps.Add(TEXT("Properties.MaxStackSize"));
        }
        if (bHasUnique) {
          GenericItem->Properties.Add(TEXT("bUniqueItem"), bUniqueItems ? TEXT("true") : TEXT("false"));
          ModifiedProps.Add(TEXT("Properties.bUniqueItem"));
        }
      }
    }

    // A success that wrote nothing read as done; the caller must reach for set_property instead.
    if (ModifiedProps.Num() == 0) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
          FString::Printf(TEXT("%s has no stacking property (bStackable, MaxStackSize or StackLimit); nothing was changed. Set the item's own fields with inspect.set_property."),
                          *ItemAsset->GetClass()->GetName()),
          TEXT("PROPERTY_NOT_FOUND"));
      return true;
    }
    ItemAsset->MarkPackageDirty();

    const bool bSaved = GetJsonBoolField(Payload, TEXT("save"), true) && McpSafeAssetSave(ItemAsset);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    if (bHasStackable) {
      Result->SetBoolField(TEXT("stackable"), bStackable);
    }
    if (bHasMax) {
      Result->SetNumberField(TEXT("maxStackSize"), MaxStackSize);
    }
    if (bHasUnique) {
      Result->SetBoolField(TEXT("uniqueItems"), bUniqueItems);
    }

    TArray<TSharedPtr<FJsonValue>> ModArr;
    for (const FString& Prop : ModifiedProps) {
      ModArr.Add(MakeShared<FJsonValueString>(Prop));
    }
    Result->SetArrayField(TEXT("modifiedProperties"), ModArr);
    Result->SetBoolField(TEXT("configured"), true);

    McpHandlerUtils::AddVerification(Result, ItemAsset);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Item stacking configured"), Result);
    return true;
  }

  if (SubAction == TEXT("set_item_icon")) {
    FString ItemPath = GetJsonStringField(Payload, TEXT("itemPath"));
    FString IconPath = GetJsonStringField(Payload, TEXT("iconPath"));

    if (ItemPath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: itemPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    UObject* ItemAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
    if (!ItemAsset) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Item not found: %s"), *ItemPath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // An empty iconPath clears the icon; any other must name an asset (it used to be stored unchecked).
    if (!IconPath.IsEmpty() && !RequireReferencedAsset(Bridge, RequestId, RequestingSocket, IconPath, TEXT("iconPath"))) {
      return true;
    }
    bool bIconSet = false;
    FString IconPropertyName;
    FString RejectedProperty;
    FString RejectedReason;

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
        if (RejectedProperty.IsEmpty()) {
          RejectedProperty = PropName;
          RejectedReason = ApplyError;
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

    // A class with no usable icon property used to answer success with iconSet:false and change nothing.
    if (!bIconSet) {
      const FString ClassName = ItemAsset->GetClass()->GetName();
      if (RejectedProperty.IsEmpty()) {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("%s has no Icon, ItemIcon, Thumbnail, DisplayIcon or InventoryIcon property to hold the icon; add one to the item class, or set the icon with inspect set_property"), *ClassName),
            TEXT("PROPERTY_NOT_FOUND"));
      } else {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("The '%s' property of %s did not accept '%s': %s"), *RejectedProperty, *ClassName, *IconPath, *RejectedReason),
            TEXT("INVALID_ARGUMENT"));
      }
      return true;
    }

    ItemAsset->MarkPackageDirty();

    const bool bSaved = GetJsonBoolField(Payload, TEXT("save"), true) && McpSafeAssetSave(ItemAsset);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetStringField(TEXT("itemPath"), ItemPath);
    Result->SetStringField(TEXT("iconPath"), IconPath);
    Result->SetBoolField(TEXT("iconSet"), true);
    Result->SetStringField(TEXT("propertyModified"), IconPropertyName);

    McpHandlerUtils::AddVerification(Result, ItemAsset);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Item icon configured"), Result);
    return true;
  }

  return false;
}

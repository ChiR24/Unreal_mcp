#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryDataAssetActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("create_item_data_asset")) {
    FString Name = GetPayloadString(Payload, TEXT("name"));
    FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Items"));

    if (Name.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: name"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    FString PathError;
    FString SanitizedName = SanitizeAssetName(Name);
    UPackage* Package = CreateValidatedInventoryAssetPackage(Path, SanitizedName, PathError);
    if (!Package) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          PathError.IsEmpty() ? TEXT("Failed to create package") : PathError,
                          TEXT("PACKAGE_CREATE_FAILED"));
      return true;
    }

    // Create UMcpGenericDataAsset (UDataAsset/UPrimaryDataAsset are abstract in UE5)
    UMcpGenericDataAsset* ItemAsset =
        NewObject<UMcpGenericDataAsset>(Package, FName(*SanitizedName), RF_Public | RF_Standalone);

    if (ItemAsset) {
      ItemAsset->MarkPackageDirty();
      FAssetRegistryModule::AssetCreated(ItemAsset);

      if (GetPayloadBool(Payload, TEXT("save"), true)) {
        McpSafeAssetSave(ItemAsset);
      }

TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("assetName"), SanitizedName);
      McpHandlerUtils::AddVerification(Result, ItemAsset);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Item data asset created"), Result);
    } else {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create item data asset"),
                          TEXT("ASSET_CREATE_FAILED"));
    }
    return true;
  }

  if (SubAction == TEXT("set_item_properties")) {
    FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));

    if (ItemPath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: itemPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the item asset and set properties (use UDataAsset base class for loading)
    UObject* Asset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
    UDataAsset* ItemAsset = Cast<UDataAsset>(Asset);

    if (!ItemAsset) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Item data asset not found: %s"), *ItemPath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    const TSharedPtr<FJsonObject>* PropertiesObj = nullptr;
    TArray<FString> ModifiedProperties;
    TArray<FString> FailedProperties;

    if (Payload->TryGetObjectField(TEXT("properties"), PropertiesObj) && PropertiesObj && (*PropertiesObj).IsValid()) {
      // Iterate through all properties in the JSON and apply them via reflection
      for (const auto& Pair : (*PropertiesObj)->Values) {
        const FString PropertyName(*Pair.Key);
        const TSharedPtr<FJsonValue>& PropertyValue = Pair.Value;

        FProperty* Prop = ItemAsset->GetClass()->FindPropertyByName(*PropertyName);
        if (Prop) {
          FString ApplyError;
          if (ApplyJsonValueToProperty(ItemAsset, Prop, PropertyValue, ApplyError)) {
            ModifiedProperties.Add(PropertyName);
          } else {
            FailedProperties.Add(FString::Printf(TEXT("%s: %s"), *PropertyName, *ApplyError));
          }
        } else if (UMcpGenericDataAsset* GenericItem = Cast<UMcpGenericDataAsset>(ItemAsset)) {
          // Generic items keep authored fields (DisplayName, Weight, Value,
          // Description, ...) in their Properties map; that is also what
          // get_inventory_info reads back.
          FString ValueText;
          if (PropertyValue.IsValid()) {
            if (PropertyValue->Type == EJson::String) {
              ValueText = PropertyValue->AsString();
            } else if (PropertyValue->Type == EJson::Number) {
              ValueText = FString::SanitizeFloat(PropertyValue->AsNumber());
            } else if (PropertyValue->Type == EJson::Boolean) {
              ValueText = PropertyValue->AsBool() ? TEXT("true") : TEXT("false");
            } else {
              TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ValueText);
              FJsonSerializer::Serialize(PropertyValue, TEXT(""), Writer, false);
            }
          }
          GenericItem->Properties.Add(PropertyName, ValueText);
          ModifiedProperties.Add(PropertyName);
        } else {
          FailedProperties.Add(FString::Printf(TEXT("%s: Property not found"), *PropertyName));
        }
      }
    }

    ItemAsset->MarkPackageDirty();

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(ItemAsset);
    }

TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("modified"), ModifiedProperties.Num() > 0);
    Result->SetNumberField(TEXT("propertiesModified"), ModifiedProperties.Num());
    McpHandlerUtils::AddVerification(Result, ItemAsset);

    TArray<TSharedPtr<FJsonValue>> ModifiedArr;
    for (const FString& Name : ModifiedProperties) {
      ModifiedArr.Add(MakeShared<FJsonValueString>(Name));
    }
    Result->SetArrayField(TEXT("modifiedProperties"), ModifiedArr);

    if (FailedProperties.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> FailedArr;
      for (const FString& Err : FailedProperties) {
        FailedArr.Add(MakeShared<FJsonValueString>(Err));
      }
      Result->SetArrayField(TEXT("failedProperties"), FailedArr);
    }

    if (ModifiedProperties.Num() == 0 && FailedProperties.Num() > 0) {
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                             TEXT("No item property could be applied"), Result,
                             TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Item properties updated"), Result);
    return true;
  }

  return false;
}

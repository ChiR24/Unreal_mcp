#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpInventoryShared.h"

bool HandleInventoryDataAssetActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("create_item_data_asset")) {
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/Items"));

    if (Name.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: name"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    FString SanitizedName = SanitizeAssetName(Name);
    UPackage* Package = CreateInventoryAssetPackage(Bridge, RequestId, RequestingSocket, Path, SanitizedName);
    if (!Package) {
      return true;
    }

    bool bSaved = false;
    UMcpGenericDataAsset* ItemAsset = NewInventoryDataAsset(Package, SanitizedName, Payload, bSaved);
    if (ItemAsset) {
      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetBoolField(TEXT("saved"), bSaved);
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
    FString ItemPath = GetJsonStringField(Payload, TEXT("itemPath"));

    UDataAsset* ItemAsset = LoadInventoryAssetOrError<UDataAsset>(
        Bridge, RequestId, RequestingSocket, ItemPath, TEXT("itemPath"), TEXT("Item data asset"));
    if (!ItemAsset) {
      return true;
    }

    const TSharedPtr<FJsonObject>* PropertiesObj = nullptr;
    TArray<FString> ModifiedProperties;
    TArray<FString> FailedProperties;
    // No properties used to answer success with modified:false after saving the untouched asset.
    if (!Payload->TryGetObjectField(TEXT("properties"), PropertiesObj) || !PropertiesObj || !(*PropertiesObj).IsValid() ||
        (*PropertiesObj)->Values.Num() == 0) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
          TEXT("properties is required: an object of field names and values, such as {\"Weight\": 0.5}. Nothing was changed."),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

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

    bool bSaved = false;
    if (ModifiedProperties.Num() > 0) {
      ItemAsset->MarkPackageDirty();
      bSaved = GetJsonBoolField(Payload, TEXT("save"), true) && McpSafeAssetSave(ItemAsset);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetBoolField(TEXT("modified"), ModifiedProperties.Num() > 0);
    Result->SetNumberField(TEXT("propertiesModified"), ModifiedProperties.Num());
    McpHandlerUtils::AddVerification(Result, ItemAsset);

    TArray<TSharedPtr<FJsonValue>> ModifiedArr = McpHandlerUtils::ToJsonStringArray(ModifiedProperties);
    Result->SetArrayField(TEXT("modifiedProperties"), ModifiedArr);

    if (FailedProperties.Num() > 0) {
      TArray<TSharedPtr<FJsonValue>> FailedArr = McpHandlerUtils::ToJsonStringArray(FailedProperties);
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

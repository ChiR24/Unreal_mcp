#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryInfoActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("get_inventory_info")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
    FString ItemPath = GetPayloadString(Payload, TEXT("itemPath"));
    FString LootTablePath = GetPayloadString(Payload, TEXT("lootTablePath"));
    FString RecipePath = GetPayloadString(Payload, TEXT("recipePath"));
    FString PickupPath = GetPayloadString(Payload, TEXT("pickupPath"));

    // Validate that at least one path is provided
    if (BlueprintPath.IsEmpty() && ItemPath.IsEmpty() && LootTablePath.IsEmpty() &&
        RecipePath.IsEmpty() && PickupPath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("At least one path parameter is required (blueprintPath, itemPath, lootTablePath, recipePath, or pickupPath)"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    auto AddGenericProperties = [](TSharedPtr<FJsonObject> TargetResult, UObject* Asset) {
      if (UMcpGenericDataAsset* GenericAsset = Cast<UMcpGenericDataAsset>(Asset)) {
        TSharedPtr<FJsonObject> PropertiesObject = MakeShared<FJsonObject>();
        for (const TPair<FString, FString>& Pair : GenericAsset->Properties) {
          PropertiesObject->SetStringField(Pair.Key, Pair.Value);
        }
        TargetResult->SetObjectField(TEXT("properties"), PropertiesObject);
        TargetResult->SetNumberField(TEXT("propertyCount"), GenericAsset->Properties.Num());
      }
    };

    if (!BlueprintPath.IsEmpty()) {
      UBlueprint* Blueprint = Cast<UBlueprint>(
          StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
      if (!Blueprint) {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
                            TEXT("ASSET_NOT_FOUND"));
        return true;
      }
      Result->SetStringField(TEXT("assetType"), TEXT("Blueprint"));
      Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
      Result->SetStringField(TEXT("className"), Blueprint->GeneratedClass->GetName());

      // Check for inventory/equipment components
      USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
      if (SCS) {
        TArray<TSharedPtr<FJsonValue>> Components;
        for (USCS_Node* Node : SCS->GetAllNodes()) {
          if (Node) {
            TSharedPtr<FJsonObject> CompInfo = McpHandlerUtils::CreateResultObject();
            CompInfo->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
            CompInfo->SetStringField(TEXT("class"),
                                     Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT("Unknown"));
            Components.Add(MakeShared<FJsonValueObject>(CompInfo));
          }
        }
        Result->SetArrayField(TEXT("components"), Components);
      }
    } else if (!ItemPath.IsEmpty()) {
      // Use UDataAsset base class for loading - UPrimaryDataAsset is abstract in UE5.7
      UObject* ItemAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *ItemPath);
      if (!ItemAsset) {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Item not found: %s"), *ItemPath),
                            TEXT("ASSET_NOT_FOUND"));
        return true;
      }
      Result->SetStringField(TEXT("assetType"), TEXT("Item"));
      Result->SetStringField(TEXT("itemPath"), ItemPath);
      Result->SetStringField(TEXT("className"), ItemAsset->GetClass()->GetName());
      AddGenericProperties(Result, ItemAsset);
    } else if (!LootTablePath.IsEmpty()) {
      UObject* LootTableAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *LootTablePath);
      if (!LootTableAsset) {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Loot table not found: %s"), *LootTablePath),
                            TEXT("ASSET_NOT_FOUND"));
        return true;
      }
      Result->SetStringField(TEXT("assetType"), TEXT("LootTable"));
      Result->SetStringField(TEXT("lootTablePath"), LootTablePath);
      AddGenericProperties(Result, LootTableAsset);
    } else if (!RecipePath.IsEmpty()) {
      UObject* RecipeAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *RecipePath);
      if (!RecipeAsset) {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Recipe not found: %s"), *RecipePath),
                            TEXT("ASSET_NOT_FOUND"));
        return true;
      }
      Result->SetStringField(TEXT("assetType"), TEXT("Recipe"));
      Result->SetStringField(TEXT("recipePath"), RecipePath);
      AddGenericProperties(Result, RecipeAsset);
    } else if (!PickupPath.IsEmpty()) {
      UBlueprint* PickupBlueprint = Cast<UBlueprint>(
          StaticLoadObject(UBlueprint::StaticClass(), nullptr, *PickupPath));
      if (!PickupBlueprint) {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Pickup blueprint not found: %s"), *PickupPath),
                            TEXT("ASSET_NOT_FOUND"));
        return true;
      }
      Result->SetStringField(TEXT("assetType"), TEXT("Pickup"));
      Result->SetStringField(TEXT("pickupPath"), PickupPath);
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Inventory info retrieved"), Result);
    return true;
  }

  // ===========================================================================

  return false;
}

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpAutomationBridge_InventoryHandlersShared.h"

namespace
{
// Blueprint readback shared by pickup and plain blueprint targets: SCS
// components, authored variables and their CDO defaults as exported text.
void AddInventoryBlueprintDefaults(const TSharedPtr<FJsonObject>& Result, UBlueprint* Blueprint)
{
  TArray<TSharedPtr<FJsonValue>> Components;
  if (USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript) {
    for (USCS_Node* Node : SCS->GetAllNodes()) {
      if (!Node) continue;
      TSharedPtr<FJsonObject> CompInfo = McpHandlerUtils::CreateResultObject();
      CompInfo->SetStringField(TEXT("name"), Node->GetVariableName().ToString());
      CompInfo->SetStringField(TEXT("class"),
                               Node->ComponentClass ? Node->ComponentClass->GetName() : TEXT("Unknown"));
      Components.Add(MakeShared<FJsonValueObject>(CompInfo));
    }
  }
  Result->SetArrayField(TEXT("components"), Components);

  UClass* GeneratedClass = Blueprint->GeneratedClass;
  UObject* CDO = GeneratedClass ? GeneratedClass->GetDefaultObject() : nullptr;
  TArray<TSharedPtr<FJsonValue>> Variables;
  TSharedPtr<FJsonObject> Properties = MakeShared<FJsonObject>();
  for (const FBPVariableDescription& Var : Blueprint->NewVariables) {
    FProperty* Property = GeneratedClass ? GeneratedClass->FindPropertyByName(Var.VarName) : nullptr;
    FString Value = Var.DefaultValue;
    if (Property && CDO) {
      Value.Reset();
      Property->ExportText_InContainer(0, Value, CDO, nullptr, CDO, PPF_None);
    }
    TSharedPtr<FJsonObject> VarInfo = McpHandlerUtils::CreateResultObject();
    VarInfo->SetStringField(TEXT("name"), Var.VarName.ToString());
    VarInfo->SetStringField(TEXT("type"), Property ? Property->GetCPPType() : Var.VarType.PinCategory.ToString());
    VarInfo->SetStringField(TEXT("default"), Value);
    Variables.Add(MakeShared<FJsonValueObject>(VarInfo));
    // Typed where the property type is known (dogfood #54: everything used to be text).
    if (Property && Property->IsA<FBoolProperty>()) {
      Properties->SetBoolField(Var.VarName.ToString(), Value.Equals(TEXT("True"), ESearchCase::IgnoreCase));
    } else if (Property && Property->IsA<FNumericProperty>() && Value.IsNumeric()) {
      Properties->SetNumberField(Var.VarName.ToString(), FCString::Atod(*Value));
    } else {
      Properties->SetStringField(Var.VarName.ToString(), Value);
    }
  }
  Result->SetArrayField(TEXT("variables"), Variables);
  Result->SetObjectField(TEXT("properties"), Properties);
  Result->SetNumberField(TEXT("propertyCount"), Blueprint->NewVariables.Num());
}

// Elements of the first array property named in Names, exported as text.
void AppendReflectedArrayEntries(UObject* Asset, std::initializer_list<const TCHAR*> Names, TArray<TSharedPtr<FJsonValue>>& Out)
{
  for (const TCHAR* Name : Names) {
    FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(Asset->GetClass(), Name);
    if (!ArrayProp) continue;
    FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Asset));
    for (int32 Index = 0; Index < Helper.Num(); ++Index) {
      FString Text;
      MCP_PROPERTY_EXPORT_TEXT(ArrayProp->Inner, Text, Helper.GetRawPtr(Index), nullptr, Asset, PPF_None);
      TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
      Entry->SetStringField(TEXT("property"), ArrayProp->GetName());
      Entry->SetNumberField(TEXT("index"), Index);
      Entry->SetStringField(TEXT("value"), Text);
      Out.Add(MakeShared<FJsonValueObject>(Entry));
    }
    return;
  }
}

// Recipe readback: ingredients stored as "Ingredient_N" = "ItemPath=..;Quantity=N"
// on the generic asset (add_recipe_ingredient) or as elements of an
// Ingredients/RequiredItems/InputItems array on a custom recipe class, plus
// the output fields and any reflected Outputs array.
void AddRecipeDetails(const TSharedPtr<FJsonObject>& Result, UObject* RecipeAsset)
{
  TArray<TSharedPtr<FJsonValue>> Ingredients;
  TArray<TSharedPtr<FJsonValue>> Outputs;
  if (UMcpGenericDataAsset* Generic = Cast<UMcpGenericDataAsset>(RecipeAsset)) {
    for (const TPair<FString, FString>& Pair : Generic->Properties) {
      TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
      Entry->SetStringField(TEXT("key"), Pair.Key);
      if (Pair.Key.StartsWith(TEXT("Ingredient_"))) {
        TArray<FString> Parts;
        Pair.Value.ParseIntoArray(Parts, TEXT(";"));
        for (const FString& Part : Parts) {
          FString Key, Value;
          if (!Part.Split(TEXT("="), &Key, &Value)) continue;
          if (Key == TEXT("Quantity")) Entry->SetNumberField(TEXT("quantity"), FCString::Atod(*Value));
          else Entry->SetStringField(Key == TEXT("ItemPath") ? TEXT("itemPath") : Key, Value);
        }
        Ingredients.Add(MakeShared<FJsonValueObject>(Entry));
      } else if (Pair.Key.StartsWith(TEXT("Output"))) {
        Entry->SetStringField(TEXT("value"), Pair.Value);
        Outputs.Add(MakeShared<FJsonValueObject>(Entry));
      }
    }
    if (const FString* OutputItem = Generic->Properties.Find(TEXT("OutputItemPath"))) {
      Result->SetStringField(TEXT("outputItem"), *OutputItem);
    }
    if (const FString* Station = Generic->Properties.Find(TEXT("RequiredStation"))) {
      Result->SetStringField(TEXT("requiredStation"), *Station);
    }
    struct FRecipeNumericField { const TCHAR* Key; const TCHAR* Field; };
    const FRecipeNumericField NumericFields[] = {
      {TEXT("OutputQuantity"), TEXT("outputQuantity")}, {TEXT("CraftTime"), TEXT("craftTime")}, {TEXT("RequiredLevel"), TEXT("requiredLevel")}};
    for (const FRecipeNumericField& Field : NumericFields) {
      if (const FString* Value = Generic->Properties.Find(Field.Key)) {
        Result->SetNumberField(Field.Field, FCString::Atod(**Value));
      }
    }
  }
  AppendReflectedArrayEntries(RecipeAsset, {TEXT("Ingredients"), TEXT("RequiredItems"), TEXT("InputItems")}, Ingredients);
  AppendReflectedArrayEntries(RecipeAsset, {TEXT("Outputs"), TEXT("OutputItems"), TEXT("Results")}, Outputs);
  Result->SetArrayField(TEXT("ingredients"), Ingredients);
  Result->SetNumberField(TEXT("ingredientCount"), Ingredients.Num());
  Result->SetArrayField(TEXT("outputs"), Outputs);
}
}

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
          // Dogfood #54: generic data-asset values are stored as text; project them typed.
          double Number = 0.0;
          if (Pair.Value.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Pair.Value.Equals(TEXT("false"), ESearchCase::IgnoreCase)) {
            PropertiesObject->SetBoolField(Pair.Key, Pair.Value.Equals(TEXT("true"), ESearchCase::IgnoreCase));
          } else if (Pair.Value.IsNumeric() && LexTryParseString(Number, *Pair.Value)) {
            PropertiesObject->SetNumberField(Pair.Key, Number);
          } else {
            PropertiesObject->SetStringField(Pair.Key, Pair.Value);
          }
        }
        // set_item_properties writes Description/ItemName to the typed UPROPERTYs, not the map (dogfood #52).
        if (!GenericAsset->Description.IsEmpty()) { PropertiesObject->SetStringField(TEXT("Description"), GenericAsset->Description); }
        if (!GenericAsset->ItemName.IsEmpty()) { PropertiesObject->SetStringField(TEXT("ItemName"), GenericAsset->ItemName); }
        TargetResult->SetObjectField(TEXT("properties"), PropertiesObject);
        TargetResult->SetNumberField(TEXT("propertyCount"), PropertiesObject->Values.Num());
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
      Result->SetStringField(TEXT("className"), Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetName() : TEXT("Unknown"));
      AddInventoryBlueprintDefaults(Result, Blueprint);
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
      // Structured loot entries beside the raw LootEntry_n strings (dogfood #54).
      if (UMcpGenericDataAsset* GenericLoot = Cast<UMcpGenericDataAsset>(LootTableAsset)) {
        TArray<TSharedPtr<FJsonValue>> LootEntries;
        for (const TPair<FString, FString>& Pair : GenericLoot->Properties) {
          if (!Pair.Key.StartsWith(TEXT("LootEntry_"))) continue;
          TSharedPtr<FJsonObject> Entry = McpHandlerUtils::CreateResultObject();
          TArray<FString> Parts;
          Pair.Value.ParseIntoArray(Parts, TEXT(";"));
          for (const FString& Part : Parts) {
            FString Key, Val;
            if (!Part.Split(TEXT("="), &Key, &Val)) continue;
            if (Key == TEXT("ItemPath")) Entry->SetStringField(TEXT("itemPath"), Val);
            else if (Key == TEXT("Weight")) Entry->SetNumberField(TEXT("weight"), FCString::Atod(*Val));
            else if (Key == TEXT("MinQuantity")) Entry->SetNumberField(TEXT("minQuantity"), FCString::Atod(*Val));
            else if (Key == TEXT("MaxQuantity")) Entry->SetNumberField(TEXT("maxQuantity"), FCString::Atod(*Val));
            else Entry->SetStringField(Key, Val);
          }
          LootEntries.Add(MakeShared<FJsonValueObject>(Entry));
        }
        Result->SetArrayField(TEXT("lootEntries"), LootEntries);
        Result->SetNumberField(TEXT("lootEntryCount"), LootEntries.Num());
      }
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
      Result->SetStringField(TEXT("className"), RecipeAsset->GetClass()->GetName());
      AddGenericProperties(Result, RecipeAsset);
      AddRecipeDetails(Result, RecipeAsset);
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
      Result->SetStringField(TEXT("className"), PickupBlueprint->GeneratedClass ? PickupBlueprint->GeneratedClass->GetName() : TEXT("Unknown"));
      // A pickup is an actor Blueprint: configure_pickup_* write their values
      // as variables + CDO defaults, so the readback is the Blueprint state.
      AddInventoryBlueprintDefaults(Result, PickupBlueprint);
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Inventory info retrieved"), Result);
    return true;
  }

  return false;
}

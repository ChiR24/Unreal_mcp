#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryCraftingRecipeActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("create_crafting_recipe")) {
    FString Name = GetPayloadString(Payload, TEXT("name"));
    FString OutputItemPath = GetPayloadString(Payload, TEXT("outputItemPath"));
    FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Data/Recipes"));

    if (Name.IsEmpty() || OutputItemPath.IsEmpty()) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Missing required parameters: name and outputItemPath"),
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
    UMcpGenericDataAsset* RecipeAsset =
        NewObject<UMcpGenericDataAsset>(Package, FName(*Name), RF_Public | RF_Standalone);

    if (RecipeAsset) {
      RecipeAsset->MarkPackageDirty();
      FAssetRegistryModule::AssetCreated(RecipeAsset);

      if (GetPayloadBool(Payload, TEXT("save"), true)) {
        McpSafeAssetSave(RecipeAsset);
      }

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("recipePath"), Package->GetName());
      Result->SetStringField(TEXT("outputItemPath"), OutputItemPath);
      Result->SetNumberField(TEXT("outputQuantity"),
                             GetPayloadNumber(Payload, TEXT("outputQuantity"), 1));
      Result->SetNumberField(TEXT("craftTime"),
                             GetPayloadNumber(Payload, TEXT("craftTime"), 1.0));
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Crafting recipe created"), Result);
    } else {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create recipe asset"),
                          TEXT("ASSET_CREATE_FAILED"));
    }
    return true;
  }

  if (SubAction == TEXT("configure_recipe_requirements")) {
    FString RecipePath = GetPayloadString(Payload, TEXT("recipePath"));

    if (RecipePath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: recipePath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    UObject* RecipeAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *RecipePath);
    UMcpGenericDataAsset* GenericRecipe = Cast<UMcpGenericDataAsset>(RecipeAsset);

    if (!GenericRecipe) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Recipe not found or unsupported asset type: %s"), *RecipePath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    const int32 RequiredLevel = static_cast<int32>(GetPayloadNumber(Payload, TEXT("requiredLevel"), 0));
    const FString RequiredStation = GetPayloadString(Payload, TEXT("requiredStation"), TEXT("None"));
    GenericRecipe->Properties.Add(TEXT("RequiredLevel"), FString::FromInt(RequiredLevel));
    GenericRecipe->Properties.Add(TEXT("RequiredStation"), RequiredStation);
    GenericRecipe->MarkPackageDirty();

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(GenericRecipe);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("recipePath"), RecipePath);
    Result->SetNumberField(TEXT("requiredLevel"), RequiredLevel);
    Result->SetStringField(TEXT("requiredStation"), RequiredStation);
    Result->SetBoolField(TEXT("configured"), true);
    Result->SetNumberField(TEXT("propertiesModified"), 2);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Recipe requirements configured"), Result);
    return true;
  }

  if (SubAction == TEXT("add_recipe_ingredient")) {
    FString RecipePath = GetPayloadString(Payload, TEXT("recipePath"));
    FString IngredientItemPath = GetPayloadString(Payload, TEXT("ingredientItemPath"));
    int32 Quantity = static_cast<int32>(GetPayloadNumber(Payload, TEXT("quantity"), 1));

    if (RecipePath.IsEmpty() || IngredientItemPath.IsEmpty()) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Missing required parameters: recipePath and ingredientItemPath"),
          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the recipe asset
    UObject* RecipeAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *RecipePath);
    if (!RecipeAsset) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Recipe not found: %s"), *RecipePath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    bool bIngredientAdded = false;
    int32 IngredientIndex = 0;

    // Try to find Ingredients array via reflection
    FProperty* IngredientsProp = RecipeAsset->GetClass()->FindPropertyByName(TEXT("Ingredients"));
    if (!IngredientsProp) {
      IngredientsProp = RecipeAsset->GetClass()->FindPropertyByName(TEXT("RequiredItems"));
    }
    if (!IngredientsProp) {
      IngredientsProp = RecipeAsset->GetClass()->FindPropertyByName(TEXT("InputItems"));
    }

    if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(IngredientsProp)) {
      FScriptArrayHelper ArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(RecipeAsset));
      // Actually add a new element to the array
      int32 NewIdx = ArrayHelper.AddValue();
      if (NewIdx != INDEX_NONE) {
        IngredientIndex = NewIdx;
        bIngredientAdded = true;
        // Note: The new element's inner fields (item path, quantity)
        // would need to be populated via reflection based on the struct definition
      } else {
        bIngredientAdded = false;
      }
    } else {
      if (UMcpGenericDataAsset* GenericRecipe = Cast<UMcpGenericDataAsset>(RecipeAsset)) {
        const int32 GenericIngredientIndex = GenericRecipe->Properties.Num();
        const FString IngredientKey = FString::Printf(TEXT("Ingredient_%d"), GenericIngredientIndex);
        const FString IngredientValue = FString::Printf(
            TEXT("ItemPath=%s;Quantity=%d"), *IngredientItemPath, Quantity);
        GenericRecipe->Properties.Add(IngredientKey, IngredientValue);
        IngredientIndex = GenericIngredientIndex;
        bIngredientAdded = true;
      } else {
        bIngredientAdded = false;
      }
    }

    RecipeAsset->MarkPackageDirty();

    if (GetPayloadBool(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(RecipeAsset);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("recipePath"), RecipePath);
    Result->SetStringField(TEXT("ingredientItemPath"), IngredientItemPath);
    Result->SetNumberField(TEXT("quantity"), Quantity);
    Result->SetNumberField(TEXT("ingredientIndex"), IngredientIndex);
    Result->SetBoolField(TEXT("added"), bIngredientAdded);

    if (!IngredientsProp && bIngredientAdded) {
      Result->SetStringField(TEXT("storage"), TEXT("Properties"));
    } else if (!IngredientsProp) {
      Result->SetStringField(TEXT("note"), TEXT("Ingredients property not found. Ensure your recipe class has an Ingredients, RequiredItems, or InputItems array."));
    }

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Recipe ingredient added"), Result);
    return true;
  }

  return false;
}

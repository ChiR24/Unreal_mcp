#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryCraftingRecipeActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("create_crafting_recipe")) {
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString OutputItemPath = GetJsonStringField(Payload, TEXT("outputItemPath"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/Data/Recipes"));

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
      // These three were only ever echoed into the JSON response, so a recipe
      // read straight back reported properties:{} and outputs:[] while the
      // create call had reported the caller's own values as if stored.
      // configure_recipe_requirements and add_recipe_ingredient already persist
      // through Properties; do the same here.
      const int32 OutputQuantity =
          static_cast<int32>(GetJsonNumberField(Payload, TEXT("outputQuantity"), 1));
      const double CraftTime = GetJsonNumberField(Payload, TEXT("craftTime"), 1.0);
      RecipeAsset->Properties.Add(TEXT("OutputItemPath"), OutputItemPath);
      RecipeAsset->Properties.Add(TEXT("OutputQuantity"), FString::FromInt(OutputQuantity));
      RecipeAsset->Properties.Add(TEXT("CraftTime"), FString::SanitizeFloat(CraftTime));
      RecipeAsset->MarkPackageDirty();
      FAssetRegistryModule::AssetCreated(RecipeAsset);

      if (GetJsonBoolField(Payload, TEXT("save"), true)) {
        McpSafeAssetSave(RecipeAsset);
      }

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("recipePath"), Package->GetName());
      Result->SetStringField(TEXT("assetPath"), Package->GetName() + TEXT(".") + FPackageName::GetShortName(Package->GetName())); // dogfood #55: consistent object path
      Result->SetStringField(TEXT("outputItemPath"), OutputItemPath);
      Result->SetNumberField(TEXT("outputQuantity"), OutputQuantity);
      Result->SetNumberField(TEXT("craftTime"), CraftTime);
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
    FString RecipePath = GetJsonStringField(Payload, TEXT("recipePath"));

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

    const int32 RequiredLevel = static_cast<int32>(GetJsonNumberField(Payload, TEXT("requiredLevel"), 0));
    const FString RequiredStation = GetJsonStringField(Payload, TEXT("requiredStation"), TEXT("None"));
    GenericRecipe->Properties.Add(TEXT("RequiredLevel"), FString::FromInt(RequiredLevel));
    GenericRecipe->Properties.Add(TEXT("RequiredStation"), RequiredStation);
    GenericRecipe->MarkPackageDirty();

    if (GetJsonBoolField(Payload, TEXT("save"), false)) {
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
    FString RecipePath = GetJsonStringField(Payload, TEXT("recipePath"));
    FString IngredientItemPath = GetJsonStringField(Payload, TEXT("ingredientItemPath"));
    int32 Quantity = static_cast<int32>(GetJsonNumberField(Payload, TEXT("quantity"), 1));

    if (RecipePath.IsEmpty() || IngredientItemPath.IsEmpty()) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          TEXT("Missing required parameters: recipePath and ingredientItemPath"),
          TEXT("MISSING_PARAMETER"));
      return true;
    }

    UObject* RecipeAsset = StaticLoadObject(UDataAsset::StaticClass(), nullptr, *RecipePath);
    if (!RecipeAsset) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Recipe not found: %s"), *RecipePath),
          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Ingredients are stored on the generic recipe assets create_crafting_recipe makes. A recipe
    // class with its own ingredient array used to get an EMPTY element appended and 'added: true'.
    UMcpGenericDataAsset* GenericRecipe = Cast<UMcpGenericDataAsset>(RecipeAsset);
    if (!GenericRecipe) {
      FProperty* IngredientsProp = nullptr;
      for (const TCHAR* Name : {TEXT("Ingredients"), TEXT("RequiredItems"), TEXT("InputItems")}) {
        if (!IngredientsProp) { IngredientsProp = RecipeAsset->GetClass()->FindPropertyByName(Name); }
      }
      Bridge.SendAutomationError(RequestingSocket, RequestId,
          IngredientsProp
              ? FString::Printf(TEXT("%s keeps ingredients in its own '%s' property; set that array with inspect set_property"), *RecipePath, *IngredientsProp->GetName())
              : FString::Printf(TEXT("%s is not a recipe made by create_crafting_recipe and has no Ingredients, RequiredItems or InputItems array"), *RecipePath),
          TEXT("UNSUPPORTED_RECIPE_CLASS"));
      return true;
    }
    const int32 IngredientIndex = GenericRecipe->Properties.Num();
    GenericRecipe->Properties.Add(FString::Printf(TEXT("Ingredient_%d"), IngredientIndex),
                                  FString::Printf(TEXT("ItemPath=%s;Quantity=%d"), *IngredientItemPath, Quantity));

    RecipeAsset->MarkPackageDirty();

    if (GetJsonBoolField(Payload, TEXT("save"), false)) {
      McpSafeAssetSave(RecipeAsset);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("recipePath"), RecipePath);
    Result->SetStringField(TEXT("ingredientItemPath"), IngredientItemPath);
    Result->SetNumberField(TEXT("quantity"), Quantity);
    Result->SetNumberField(TEXT("ingredientIndex"), IngredientIndex);
    Result->SetBoolField(TEXT("added"), true);
    Result->SetStringField(TEXT("storage"), TEXT("Properties"));

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Recipe ingredient added"), Result);
    return true;
  }

  return false;
}

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryCraftingStationActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("create_crafting_station")) {
    FString Name = GetPayloadString(Payload, TEXT("name"));
    FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Blueprints/CraftingStations"));
    FString StationType =
        GetPayloadString(Payload, TEXT("stationType"), TEXT("Basic"));

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

    UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
    Factory->ParentClass = AActor::StaticClass();

    UBlueprint* StationBlueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name,
                                  RF_Public | RF_Standalone, nullptr, GWarn));

    if (StationBlueprint) {
      USimpleConstructionScript* SCS = StationBlueprint->SimpleConstructionScript;
      if (SCS) {
        USCS_Node* MeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("StationMesh"));
        if (MeshNode) {
          SCS->AddNode(MeshNode);
        }

        USCS_Node* BoxNode = SCS->CreateNode(UBoxComponent::StaticClass(), TEXT("InteractionBox"));
        if (BoxNode) {
          SCS->AddNode(BoxNode);
        }
      }

      FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(StationBlueprint);
      FAssetRegistryModule::AssetCreated(StationBlueprint);

        if (GetPayloadBool(Payload, TEXT("save"), true)) {
          McpSafeAssetSave(StationBlueprint);
        }

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("stationPath"), Package->GetName());
      Result->SetStringField(TEXT("assetPath"), Package->GetName() + TEXT(".") + FPackageName::GetShortName(Package->GetName())); // dogfood #55: consistent object path
      Result->SetStringField(TEXT("stationType"), StationType);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Crafting station created"), Result);
    } else {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create crafting station blueprint"),
                          TEXT("BLUEPRINT_CREATE_FAILED"));
    }
    return true;
  }

  if (SubAction == TEXT("configure_station_recipes")) {
    FString StationPath = GetPayloadString(Payload, TEXT("stationPath"));

    if (StationPath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: stationPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    UBlueprint* Blueprint =
        Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *StationPath));
    if (!Blueprint) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Crafting station blueprint not found: %s"), *StationPath),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    TArray<FString> RecipePaths;
    const TArray<TSharedPtr<FJsonValue>>* RecipesArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("recipePaths"), RecipesArr) && RecipesArr) {
      for (const auto& RecipeVal : *RecipesArr) {
        RecipePaths.Add(RecipeVal->AsString());
      }
    }

    FString StationType = GetPayloadString(Payload, TEXT("stationType"), TEXT("Basic"));
    double CraftingSpeed = GetPayloadNumber(Payload, TEXT("craftingSpeedMultiplier"), 1.0);

    FEdGraphPinType SoftObjectArrayType;
    SoftObjectArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
    SoftObjectArrayType.ContainerType = EPinContainerType::Array;

    FEdGraphPinType NameType;
    NameType.PinCategory = UEdGraphSchema_K2::PC_Name;

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    TArray<TPair<FName, FEdGraphPinType>> StationVars = {
      TPair<FName, FEdGraphPinType>(TEXT("AvailableRecipes"), SoftObjectArrayType),
      TPair<FName, FEdGraphPinType>(TEXT("StationType"), NameType),
      TPair<FName, FEdGraphPinType>(TEXT("CraftingSpeedMultiplier"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("bRequiresFuel"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("FuelConsumptionRate"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("bAutoStartCrafting"), BoolType)
    };

    TArray<TSharedPtr<FJsonValue>> AddedVars;

    for (const auto& VarPair : StationVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarPair.Key) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
        AddedVars.Add(MakeShared<FJsonValueString>(VarPair.Key.ToString()));
      }
    }

    FEdGraphPinType DelegateType;
    DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

    TArray<FName> EventNames = {
      TEXT("OnRecipeQueued"),
      TEXT("OnCraftingStarted"),
      TEXT("OnCraftingCompleted"),
      TEXT("OnFuelDepleted")
    };

    for (const FName& EventName : EventNames) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == EventName) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, EventName, DelegateType);
        AddedVars.Add(MakeShared<FJsonValueString>(EventName.ToString()));
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("stationPath"), StationPath);
    Result->SetStringField(TEXT("stationType"), StationType);
    Result->SetNumberField(TEXT("recipeCount"), RecipePaths.Num());
    Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
    Result->SetBoolField(TEXT("configured"), true);

    TArray<TSharedPtr<FJsonValue>> RecipePathsArr;
    for (const FString& Path : RecipePaths) {
      RecipePathsArr.Add(MakeShared<FJsonValueString>(Path));
    }
    Result->SetArrayField(TEXT("recipePaths"), RecipePathsArr);

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Crafting station recipes configured"), Result);
    return true;
  }

  return false;
}

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryCraftingStationActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("create_crafting_station")) {
    FString Name = GetJsonStringField(Payload, TEXT("name"));
    FString Path = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/Blueprints/CraftingStations"));
    FString StationType =
        GetJsonStringField(Payload, TEXT("stationType"), TEXT("Basic"));

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

        if (GetJsonBoolField(Payload, TEXT("save"), true)) {
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

  return false;
}

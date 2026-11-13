#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Blueprints/McpScsLookup.h"
#include "Domains/Inventory/McpInventoryShared.h"
#include "Foundation/BridgeHelpers/Blueprints/McpNewBlueprint.h"

bool HandleInventoryCraftingStationActions(UEngineWireSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
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

    UPackage* Package = CreateInventoryAssetPackage(Bridge, RequestId, RequestingSocket, Path, Name);
    if (!Package) {
      return true;
    }

    UBlueprint* StationBlueprint = McpNewBlueprint(Package, *Name, AActor::StaticClass());

    if (StationBlueprint) {
      USimpleConstructionScript* SCS = StationBlueprint->SimpleConstructionScript;
      if (SCS) {
        McpAddScsNode(SCS, UStaticMeshComponent::StaticClass(), TEXT("StationMesh"));
        McpAddScsNode(SCS, UBoxComponent::StaticClass(), TEXT("InteractionBox"));
      }

      FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(StationBlueprint);
      FAssetRegistryModule::AssetCreated(StationBlueprint);

      const bool bSaved = GetJsonBoolField(Payload, TEXT("save"), true) && McpSafeAssetSave(StationBlueprint);

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetBoolField(TEXT("saved"), bSaved);
      Result->SetStringField(TEXT("stationPath"), Package->GetName());
      Result->SetStringField(TEXT("assetPath"), McpToObjectPath(Package->GetName())); // consistent object path
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

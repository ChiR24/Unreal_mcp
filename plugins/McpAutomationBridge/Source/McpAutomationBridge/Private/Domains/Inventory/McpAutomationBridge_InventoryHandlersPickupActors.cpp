#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryPickupActorActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("create_pickup_actor")) {
    FString Name = GetPayloadString(Payload, TEXT("name"));
    FString Path = GetPayloadString(Payload, TEXT("path"), TEXT("/Game/Blueprints/Pickups"));

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

    UBlueprint* NewBlueprint = Cast<UBlueprint>(
        Factory->FactoryCreateNew(UBlueprint::StaticClass(), Package, *Name,
                                  RF_Public | RF_Standalone, nullptr, GWarn));

    if (NewBlueprint) {
      USimpleConstructionScript* SCS = NewBlueprint->SimpleConstructionScript;
      if (SCS) {
        USCS_Node* MeshNode = SCS->CreateNode(UStaticMeshComponent::StaticClass(), TEXT("PickupMesh"));
        if (MeshNode) {
          SCS->AddNode(MeshNode);
        }

        USCS_Node* SphereNode = SCS->CreateNode(USphereComponent::StaticClass(), TEXT("InteractionSphere"));
        if (SphereNode) {
          SCS->AddNode(SphereNode);
          USphereComponent* SphereComp = Cast<USphereComponent>(SphereNode->ComponentTemplate);
          if (SphereComp) {
            SphereComp->SetSphereRadius(100.0f);
            SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
          }
        }
      }

      FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(NewBlueprint);
      FAssetRegistryModule::AssetCreated(NewBlueprint);

      if (GetPayloadBool(Payload, TEXT("save"), true)) {
        McpSafeAssetSave(NewBlueprint);
      }

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("pickupPath"), Package->GetName());
      Result->SetStringField(TEXT("blueprintName"), Name);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Pickup actor created"), Result);
    } else {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create pickup blueprint"),
                          TEXT("BLUEPRINT_CREATE_FAILED"));
    }
    return true;
  }

  if (SubAction == TEXT("configure_pickup_interaction")) {
    FString PickupPath = GetPayloadString(Payload, TEXT("pickupPath"));
    FString InteractionType =
        GetPayloadString(Payload, TEXT("interactionType"), TEXT("Overlap"));
    FString Prompt = GetPayloadString(Payload, TEXT("prompt"), TEXT("Press E to pick up"));

    if (PickupPath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: pickupPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    UBlueprint* Blueprint =
        Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *PickupPath));
    if (!Blueprint) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Pickup blueprint not found: %s"), *PickupPath),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    bool bConfigured = false;

    FEdGraphPinType StringType;
    StringType.PinCategory = UEdGraphSchema_K2::PC_String;

    FEdGraphPinType NameType;
    NameType.PinCategory = UEdGraphSchema_K2::PC_Name;

    bool bInteractionTypeExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("InteractionType")) {
        bInteractionTypeExists = true;
        break;
      }
    }
    if (!bInteractionTypeExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("InteractionType"), NameType);
    }

    bool bPromptExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("InteractionPrompt")) {
        bPromptExists = true;
        break;
      }
    }
    if (!bPromptExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("InteractionPrompt"), StringType);
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (SCS) {
      for (USCS_Node* Node : SCS->GetAllNodes()) {
        if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf(USphereComponent::StaticClass())) {
          USphereComponent* SphereComp = Cast<USphereComponent>(Node->ComponentTemplate);
          if (SphereComp) {
            if (InteractionType.Equals(TEXT("Overlap"), ESearchCase::IgnoreCase)) {
              SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
              SphereComp->SetGenerateOverlapEvents(true);
            } else {
              SphereComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            }
            bConfigured = true;
          }
        }
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("pickupPath"), PickupPath);
    Result->SetStringField(TEXT("interactionType"), InteractionType);
    Result->SetStringField(TEXT("prompt"), Prompt);
    Result->SetBoolField(TEXT("configured"), bConfigured);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Pickup interaction configured"), Result);
    return true;
  }

  return false;
}

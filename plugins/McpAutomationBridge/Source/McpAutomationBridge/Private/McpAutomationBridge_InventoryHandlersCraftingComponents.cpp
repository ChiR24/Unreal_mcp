#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryCraftingComponentActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("add_crafting_component")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
    FString ComponentName =
        GetPayloadString(Payload, TEXT("componentName"), TEXT("CraftingComponent"));

    if (BlueprintPath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: blueprintPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    UBlueprint* Blueprint =
        Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
    if (!Blueprint) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
    if (SCS) {
      // Use USceneComponent for proper SCS hierarchy (UActorComponent cannot be added to SCS)
      USCS_Node* NewNode = SCS->CreateNode(USceneComponent::StaticClass(), *ComponentName);
      if (NewNode) {
        SCS->AddNode(NewNode);

        // Add crafting-related Blueprint variables
        FEdGraphPinType SoftObjectArrayType;
        SoftObjectArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
        SoftObjectArrayType.ContainerType = EPinContainerType::Array;

        FEdGraphPinType BoolType;
        BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

        FEdGraphPinType FloatType;
        FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
        FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

        FEdGraphPinType IntType;
        IntType.PinCategory = UEdGraphSchema_K2::PC_Int;

        // Crafting variables
        TArray<TPair<FName, FEdGraphPinType>> CraftingVars = {
          TPair<FName, FEdGraphPinType>(TEXT("AvailableRecipes"), SoftObjectArrayType),
          TPair<FName, FEdGraphPinType>(TEXT("CraftingQueue"), SoftObjectArrayType),
          TPair<FName, FEdGraphPinType>(TEXT("bIsCrafting"), BoolType),
          TPair<FName, FEdGraphPinType>(TEXT("CurrentCraftProgress"), FloatType),
          TPair<FName, FEdGraphPinType>(TEXT("CraftingSpeedMultiplier"), FloatType),
          TPair<FName, FEdGraphPinType>(TEXT("MaxQueueSize"), IntType)
        };

        TArray<TSharedPtr<FJsonValue>> AddedVars;

        for (const auto& VarPair : CraftingVars) {
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

        // Add event dispatchers for crafting
        FEdGraphPinType DelegateType;
        DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

        TArray<FName> EventNames = {
          TEXT("OnCraftingStarted"),
          TEXT("OnCraftingCompleted"),
          TEXT("OnCraftingCancelled"),
          TEXT("OnCraftingProgressUpdated")
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
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetBoolField(TEXT("componentAdded"), true);
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Crafting component added"), Result);
        return true;
      }
    }

    Bridge.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create crafting component"),
                        TEXT("COMPONENT_CREATE_FAILED"));
    return true;
  }

  // ===========================================================================
  // 17.7 Additional Actions (6 actions to complete 33 total)
  // ===========================================================================

  return false;
}

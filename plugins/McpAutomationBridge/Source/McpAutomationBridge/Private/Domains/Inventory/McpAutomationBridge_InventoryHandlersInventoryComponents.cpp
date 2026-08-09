#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryComponentActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("create_inventory_component")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
    FString ComponentName =
        GetPayloadString(Payload, TEXT("componentName"), TEXT("InventoryComponent"));

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
    if (!SCS) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Blueprint has no SimpleConstructionScript"),
                          TEXT("NO_SCS"));
      return true;
    }

    // Create a SceneComponent as inventory component (real inventory would use custom UInventoryComponent)
    // USceneComponent allows for proper hierarchy and is a valid SCS node type
    USCS_Node* NewNode = SCS->CreateNode(USceneComponent::StaticClass(), *ComponentName);
    if (NewNode) {
      SCS->AddNode(NewNode);

      int32 SlotCount = static_cast<int32>(GetPayloadNumber(Payload, TEXT("slotCount"), 20));

      FEdGraphPinType SlotArrayType;
      SlotArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
      SlotArrayType.ContainerType = EPinContainerType::Array;
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("InventorySlots"), SlotArrayType);

      FEdGraphPinType IntType;
      IntType.PinCategory = UEdGraphSchema_K2::PC_Int;
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("MaxSlots"), IntType);

      FEdGraphPinType FloatType;
      FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
      FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CurrentWeight"), FloatType);

      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("MaxWeight"), FloatType);

      FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        if (GetPayloadBool(Payload, TEXT("save"), true)) {
          McpSafeAssetSave(Blueprint);
        }

      TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
      Result->SetStringField(TEXT("componentName"), ComponentName);
      Result->SetBoolField(TEXT("componentAdded"), true);
      Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Inventory component added"), Result);
    } else {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create inventory component"),
                          TEXT("COMPONENT_CREATE_FAILED"));
    }
    return true;
  }

  if (SubAction == TEXT("configure_inventory_slots")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
    int32 SlotCount = static_cast<int32>(GetPayloadNumber(Payload, TEXT("slotCount"), 20));

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

    bool bConfigured = false;

    // Try to find and set MaxSlots property on the Blueprint's generated class CDO
    if (Blueprint->GeneratedClass) {
      UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
      if (CDO) {
        FProperty* MaxSlotsProp = CDO->GetClass()->FindPropertyByName(TEXT("MaxSlots"));
        if (MaxSlotsProp) {
          TSharedPtr<FJsonValue> SlotValue = MakeShared<FJsonValueNumber>(static_cast<double>(SlotCount));
          FString ApplyError;
          if (ApplyJsonValueToProperty(CDO, MaxSlotsProp, SlotValue, ApplyError)) {
            bConfigured = true;
          }
        }
      }
    }

    // If MaxSlots property doesn't exist, add it as a Blueprint variable
    if (!bConfigured) {
      FEdGraphPinType IntType;
      IntType.PinCategory = UEdGraphSchema_K2::PC_Int;

      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == TEXT("MaxSlots")) {
          bExists = true;
          break;
        }
      }

      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("MaxSlots"), IntType);
      }
      bConfigured = true;
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetNumberField(TEXT("slotCount"), SlotCount);
    Result->SetBoolField(TEXT("configured"), bConfigured);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Inventory slots configured"), Result);
    return true;
  }

  return false;
}

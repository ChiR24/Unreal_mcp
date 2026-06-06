#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryEquipmentComponentActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("create_equipment_component")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
    FString ComponentName =
        GetPayloadString(Payload, TEXT("componentName"), TEXT("EquipmentComponent"));

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
      // Create a SceneComponent for equipment (proper hierarchy support)
      USCS_Node* NewNode = SCS->CreateNode(USceneComponent::StaticClass(), *ComponentName);
      if (NewNode) {
        SCS->AddNode(NewNode);

        // Add equipment-related Blueprint variables
        FEdGraphPinType SoftObjectArrayType;
        SoftObjectArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
        SoftObjectArrayType.ContainerType = EPinContainerType::Array;

        FEdGraphPinType NameArrayType;
        NameArrayType.PinCategory = UEdGraphSchema_K2::PC_Name;
        NameArrayType.ContainerType = EPinContainerType::Array;

        // Add EquipmentSlots array variable
        bool bSlotsExists = false;
        for (FBPVariableDescription& Var : Blueprint->NewVariables) {
          if (Var.VarName == TEXT("EquipmentSlots")) {
            bSlotsExists = true;
            break;
          }
        }
        if (!bSlotsExists) {
          FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("EquipmentSlots"), SoftObjectArrayType);
        }

        // Add EquippedItems array
        bool bEquippedExists = false;
        for (FBPVariableDescription& Var : Blueprint->NewVariables) {
          if (Var.VarName == TEXT("EquippedItems")) {
            bEquippedExists = true;
            break;
          }
        }
        if (!bEquippedExists) {
          FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("EquippedItems"), SoftObjectArrayType);
        }

        // Add SlotNames array
        bool bSlotNamesExists = false;
        for (FBPVariableDescription& Var : Blueprint->NewVariables) {
          if (Var.VarName == TEXT("SlotNames")) {
            bSlotNamesExists = true;
            break;
          }
        }
        if (!bSlotNamesExists) {
          FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("SlotNames"), NameArrayType);
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        if (GetPayloadBool(Payload, TEXT("save"), true)) {
          McpSafeAssetSave(Blueprint);
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("componentName"), ComponentName);
        Result->SetBoolField(TEXT("componentAdded"), true);
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);

        TArray<TSharedPtr<FJsonValue>> AddedVars;
        AddedVars.Add(MakeShared<FJsonValueString>(TEXT("EquipmentSlots")));
        AddedVars.Add(MakeShared<FJsonValueString>(TEXT("EquippedItems")));
        AddedVars.Add(MakeShared<FJsonValueString>(TEXT("SlotNames")));
        Result->SetArrayField(TEXT("variablesAdded"), AddedVars);

        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Equipment component added"), Result);
        return true;
      }
    }

    Bridge.SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Failed to create equipment component"),
                        TEXT("COMPONENT_CREATE_FAILED"));
    return true;
  }

  if (SubAction == TEXT("define_equipment_slots")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

    if (BlueprintPath.IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Missing required parameter: blueprintPath"),
                          TEXT("MISSING_PARAMETER"));
      return true;
    }

    // Load the blueprint
    UBlueprint* Blueprint =
        Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *BlueprintPath));
    if (!Blueprint) {
      Bridge.SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
          TEXT("BLUEPRINT_NOT_FOUND"));
      return true;
    }

    // Get custom slots from payload or use defaults
    TArray<FString> SlotNames;
    const TArray<TSharedPtr<FJsonValue>>* SlotsArr = nullptr;
    if (Payload->TryGetArrayField(TEXT("slots"), SlotsArr) && SlotsArr) {
      for (const auto& SlotVal : *SlotsArr) {
        SlotNames.Add(SlotVal->AsString());
      }
    }

    // Default slots if none provided
    if (SlotNames.Num() == 0) {
      SlotNames = {
        TEXT("Head"),
        TEXT("Chest"),
        TEXT("Hands"),
        TEXT("Legs"),
        TEXT("Feet"),
        TEXT("MainWeapon"),
        TEXT("OffhandWeapon")
      };
    }

    // Add SlotNames array variable if it doesn't exist
    FEdGraphPinType NameArrayType;
    NameArrayType.PinCategory = UEdGraphSchema_K2::PC_Name;
    NameArrayType.ContainerType = EPinContainerType::Array;

    bool bSlotNamesExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("SlotNames")) {
        bSlotNamesExists = true;
        break;
      }
    }
    if (!bSlotNamesExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("SlotNames"), NameArrayType);
    }

    // Add EquippedItems array (parallel array to SlotNames)
    FEdGraphPinType SoftObjectArrayType;
    SoftObjectArrayType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
    SoftObjectArrayType.ContainerType = EPinContainerType::Array;

    bool bEquippedExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("EquippedItems")) {
        bEquippedExists = true;
        break;
      }
    }
    if (!bEquippedExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("EquippedItems"), SoftObjectArrayType);
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);

    TArray<TSharedPtr<FJsonValue>> ConfiguredSlots;
    for (const FString& SlotName : SlotNames) {
      ConfiguredSlots.Add(MakeShared<FJsonValueString>(SlotName));
    }
    Result->SetArrayField(TEXT("slotsConfigured"), ConfiguredSlots);
    Result->SetNumberField(TEXT("slotCount"), SlotNames.Num());

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Equipment slots defined"), Result);
    return true;
  }

  return false;
}

#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryEquipmentFunctionActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("add_equipment_functions")) {
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

    TArray<TSharedPtr<FJsonValue>> FunctionsAdded;
    TArray<TSharedPtr<FJsonValue>> VariablesAdded;

    // Add helper variables for equipment operations
    FEdGraphPinType IntType;
    IntType.PinCategory = UEdGraphSchema_K2::PC_Int;

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType NameType;
    NameType.PinCategory = UEdGraphSchema_K2::PC_Name;

    FEdGraphPinType SoftObjectType;
    SoftObjectType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;

    // Add variables that support equipment functions
    TArray<TPair<FName, FEdGraphPinType>> EquipmentVars = {
      TPair<FName, FEdGraphPinType>(TEXT("LastEquippedSlot"), NameType),
      TPair<FName, FEdGraphPinType>(TEXT("LastUnequippedSlot"), NameType),
      TPair<FName, FEdGraphPinType>(TEXT("bLastEquipSuccess"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("CurrentlyEquippedItem"), SoftObjectType),
      TPair<FName, FEdGraphPinType>(TEXT("PendingEquipItem"), SoftObjectType),
      TPair<FName, FEdGraphPinType>(TEXT("EquipmentChangeCount"), IntType)
    };

    for (const auto& VarPair : EquipmentVars) {
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName == VarPair.Key) {
          bExists = true;
          break;
        }
      }
      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarPair.Key, VarPair.Value);
        VariablesAdded.Add(MakeShared<FJsonValueString>(VarPair.Key.ToString()));
      }
    }

    // Add event dispatchers for equipment operations
    FEdGraphPinType DelegateType;
    DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

    TArray<FName> EventNames = {
      TEXT("OnEquipItemRequested"),
      TEXT("OnUnequipItemRequested"),
      TEXT("OnEquipmentSwapRequested"),
      TEXT("OnEquipmentChanged")
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
        FunctionsAdded.Add(MakeShared<FJsonValueString>(EventName.ToString()));
      }
    }

    // Mark as expected functions to implement in Blueprint
    TArray<FString> FunctionStubs = {
      TEXT("EquipItem"),
      TEXT("UnequipItem"),
      TEXT("GetEquippedItem"),
      TEXT("CanEquip"),
      TEXT("SwapEquipment")
    };

    for (const FString& FuncName : FunctionStubs) {
      FunctionsAdded.Add(MakeShared<FJsonValueString>(FuncName + TEXT(" (implement in Blueprint)")));
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("functionsAdded"), FunctionsAdded);
    Result->SetArrayField(TEXT("variablesAdded"), VariablesAdded);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetStringField(TEXT("note"), TEXT("Helper variables and event dispatchers added. Implement function logic in Blueprint graph."));

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Equipment functions added"), Result);
    return true;
  }

  return false;
}

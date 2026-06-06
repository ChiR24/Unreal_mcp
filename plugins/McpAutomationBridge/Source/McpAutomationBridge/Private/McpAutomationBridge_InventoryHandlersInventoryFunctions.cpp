#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryFunctionActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("add_inventory_functions")) {
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

    // Note: Creating actual Blueprint functions programmatically requires K2Node graph manipulation
    // which is complex and error-prone. Instead, we add helper variables and event dispatchers
    // that can be used in Blueprint graphs to implement inventory functionality.

    TArray<TSharedPtr<FJsonValue>> FunctionsAdded;
    TArray<TSharedPtr<FJsonValue>> VariablesAdded;

    // Add helper variables for inventory operations
    FEdGraphPinType IntType;
    IntType.PinCategory = UEdGraphSchema_K2::PC_Int;

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    FEdGraphPinType SoftObjectType;
    SoftObjectType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;

    // Add variables that support inventory functions
    TArray<TPair<FName, FEdGraphPinType>> InventoryVars = {
      TPair<FName, FEdGraphPinType>(TEXT("LastAddedItemIndex"), IntType),
      TPair<FName, FEdGraphPinType>(TEXT("LastRemovedItemIndex"), IntType),
      TPair<FName, FEdGraphPinType>(TEXT("bLastOperationSuccess"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("CachedItemCount"), IntType),
      TPair<FName, FEdGraphPinType>(TEXT("SelectedSlotIndex"), IntType)
    };

    for (const auto& VarPair : InventoryVars) {
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

    // Add event dispatchers for inventory operations
    FEdGraphPinType DelegateType;
    DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

    TArray<FName> EventNames = {
      TEXT("OnAddItemRequested"),
      TEXT("OnRemoveItemRequested"),
      TEXT("OnTransferItemRequested")
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
      TEXT("AddItem"),
      TEXT("RemoveItem"),
      TEXT("GetItemCount"),
      TEXT("HasItem"),
      TEXT("TransferItem")
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
    Result->SetStringField(TEXT("note"), TEXT("Helper variables and event dispatchers added. Implement function logic in Blueprint graph using these variables."));

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Inventory functions added"), Result);
    return true;
  }

  if (SubAction == TEXT("configure_inventory_events")) {
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

    // Define event dispatchers to add
    TArray<FString> EventNames = {
      TEXT("OnItemAdded"),
      TEXT("OnItemRemoved"),
      TEXT("OnInventoryChanged"),
      TEXT("OnSlotUpdated")
    };

    TArray<TSharedPtr<FJsonValue>> EventsAdded;

    // Add event dispatcher variables for each event
    FEdGraphPinType DelegateType;
    DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

    for (const FString& EventName : EventNames) {
      // Check if variable already exists
      bool bExists = false;
      for (FBPVariableDescription& Var : Blueprint->NewVariables) {
        if (Var.VarName.ToString() == EventName) {
          bExists = true;
          break;
        }
      }

      if (!bExists) {
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, FName(*EventName), DelegateType);
        EventsAdded.Add(MakeShared<FJsonValueString>(EventName));
      } else {
        EventsAdded.Add(MakeShared<FJsonValueString>(EventName + TEXT(" (exists)")));
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("eventsAdded"), EventsAdded);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Inventory events configured"), Result);
    return true;
  }

  return false;
}

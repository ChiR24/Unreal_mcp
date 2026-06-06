#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryReplicationActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("set_inventory_replication")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));
    bool bReplicated = GetPayloadBool(Payload, TEXT("replicated"), false);
    FString ReplicationCondition = GetPayloadString(Payload, TEXT("replicationCondition"), TEXT("None"));

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

    TArray<FString> ReplicatedVariables;

    // Find inventory-related variables and set their replication flags
    TArray<FName> InventoryVarNames = {
      TEXT("InventorySlots"),
      TEXT("MaxSlots"),
      TEXT("CurrentWeight"),
      TEXT("MaxWeight")
    };

    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      bool bIsInventoryVar = false;
      for (const FName& VarName : InventoryVarNames) {
        if (Var.VarName == VarName) {
          bIsInventoryVar = true;
          break;
        }
      }

      if (bIsInventoryVar) {
        if (bReplicated) {
          Var.PropertyFlags |= CPF_Net;
          Var.RepNotifyFunc = NAME_None; // Can be set to a custom function name

          // Set replication condition
          if (ReplicationCondition.Equals(TEXT("OwnerOnly"), ESearchCase::IgnoreCase)) {
            Var.ReplicationCondition = COND_OwnerOnly;
          } else if (ReplicationCondition.Equals(TEXT("SkipOwner"), ESearchCase::IgnoreCase)) {
            Var.ReplicationCondition = COND_SkipOwner;
          } else if (ReplicationCondition.Equals(TEXT("SimulatedOnly"), ESearchCase::IgnoreCase)) {
            Var.ReplicationCondition = COND_SimulatedOnly;
          } else if (ReplicationCondition.Equals(TEXT("AutonomousOnly"), ESearchCase::IgnoreCase)) {
            Var.ReplicationCondition = COND_AutonomousOnly;
          } else if (ReplicationCondition.Equals(TEXT("SimulatedOrPhysics"), ESearchCase::IgnoreCase)) {
            Var.ReplicationCondition = COND_SimulatedOrPhysics;
          } else if (ReplicationCondition.Equals(TEXT("InitialOrOwner"), ESearchCase::IgnoreCase)) {
            Var.ReplicationCondition = COND_InitialOrOwner;
          } else if (ReplicationCondition.Equals(TEXT("Custom"), ESearchCase::IgnoreCase)) {
            Var.ReplicationCondition = COND_Custom;
          } else {
            Var.ReplicationCondition = COND_None;
          }
        } else {
          Var.PropertyFlags &= ~CPF_Net;
          Var.ReplicationCondition = COND_None;
        }
        ReplicatedVariables.Add(Var.VarName.ToString());
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("replicated"), bReplicated);
    Result->SetStringField(TEXT("replicationCondition"), ReplicationCondition);
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);

    TArray<TSharedPtr<FJsonValue>> VarsArr;
    for (const FString& VarName : ReplicatedVariables) {
      VarsArr.Add(MakeShared<FJsonValueString>(VarName));
    }
    Result->SetArrayField(TEXT("modifiedVariables"), VarsArr);

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Inventory replication configured"), Result);
    return true;
  }

  // ===========================================================================
  // 17.3 Pickups (4 actions)
  // ===========================================================================

  if (SubAction == TEXT("configure_inventory_weight")) {
    FString BlueprintPath = GetPayloadString(Payload, TEXT("blueprintPath"));

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

    double MaxWeight = GetPayloadNumber(Payload, TEXT("maxWeight"), 100.0);
    bool bEnableWeight = GetPayloadBool(Payload, TEXT("enableWeight"), true);
    bool bEncumberanceSystem = GetPayloadBool(Payload, TEXT("encumberanceSystem"), false);
    double EncumberanceThreshold = GetPayloadNumber(Payload, TEXT("encumberanceThreshold"), 0.75);

    FEdGraphPinType FloatType;
    FloatType.PinCategory = UEdGraphSchema_K2::PC_Real;
    FloatType.PinSubCategory = UEdGraphSchema_K2::PC_Float;

    FEdGraphPinType BoolType;
    BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;

    // Weight configuration variables
    TArray<TPair<FName, FEdGraphPinType>> WeightVars = {
      TPair<FName, FEdGraphPinType>(TEXT("MaxCarryWeight"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("CurrentCarryWeight"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("bWeightEnabled"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("bUseEncumberance"), BoolType),
      TPair<FName, FEdGraphPinType>(TEXT("EncumberanceThreshold"), FloatType),
      TPair<FName, FEdGraphPinType>(TEXT("WeightMultiplier"), FloatType)
    };

    TArray<TSharedPtr<FJsonValue>> AddedVars;

    for (const auto& VarPair : WeightVars) {
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

    // Add weight-related event
    FEdGraphPinType DelegateType;
    DelegateType.PinCategory = UEdGraphSchema_K2::PC_MCDelegate;

    bool bEventExists = false;
    for (FBPVariableDescription& Var : Blueprint->NewVariables) {
      if (Var.VarName == TEXT("OnEncumberanceChanged")) {
        bEventExists = true;
        break;
      }
    }
    if (!bEventExists) {
      FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("OnEncumberanceChanged"), DelegateType);
      AddedVars.Add(MakeShared<FJsonValueString>(TEXT("OnEncumberanceChanged")));
    }

    // Set default values on CDO if available
    if (Blueprint->GeneratedClass) {
      UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
      if (CDO) {
        FProperty* MaxWeightProp = CDO->GetClass()->FindPropertyByName(TEXT("MaxCarryWeight"));
        if (MaxWeightProp) {
          TSharedPtr<FJsonValue> FloatVal = MakeShared<FJsonValueNumber>(MaxWeight);
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, MaxWeightProp, FloatVal, ApplyError);
        }

        FProperty* EnableProp = CDO->GetClass()->FindPropertyByName(TEXT("bWeightEnabled"));
        if (EnableProp) {
          TSharedPtr<FJsonValue> BoolVal = MakeShared<FJsonValueBoolean>(bEnableWeight);
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, EnableProp, BoolVal, ApplyError);
        }

        FProperty* EncumProp = CDO->GetClass()->FindPropertyByName(TEXT("bUseEncumberance"));
        if (EncumProp) {
          TSharedPtr<FJsonValue> BoolVal = MakeShared<FJsonValueBoolean>(bEncumberanceSystem);
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, EncumProp, BoolVal, ApplyError);
        }

        FProperty* ThreshProp = CDO->GetClass()->FindPropertyByName(TEXT("EncumberanceThreshold"));
        if (ThreshProp) {
          TSharedPtr<FJsonValue> FloatVal = MakeShared<FJsonValueNumber>(EncumberanceThreshold);
          FString ApplyError;
          ApplyJsonValueToProperty(CDO, ThreshProp, FloatVal, ApplyError);
        }
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetPayloadBool(Payload, TEXT("save"), true)) {
      McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("maxWeight"), MaxWeight);
    Result->SetBoolField(TEXT("enableWeight"), bEnableWeight);
    Result->SetBoolField(TEXT("encumberanceSystem"), bEncumberanceSystem);
    Result->SetNumberField(TEXT("encumberanceThreshold"), EncumberanceThreshold);
    Result->SetArrayField(TEXT("variablesAdded"), AddedVars);
    Result->SetBoolField(TEXT("configured"), true);

    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Inventory weight configured"), Result);
    return true;
  }

  return false;
}

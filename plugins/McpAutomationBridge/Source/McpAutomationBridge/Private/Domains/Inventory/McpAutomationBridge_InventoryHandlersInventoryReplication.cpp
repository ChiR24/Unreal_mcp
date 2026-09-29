#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/Inventory/McpAutomationBridge_InventoryHandlersShared.h"

bool HandleInventoryReplicationActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (SubAction == TEXT("set_inventory_replication")) {
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    bool bReplicated = GetJsonBoolField(Payload, TEXT("replicated"), false);
    FString ReplicationCondition = GetJsonStringField(Payload, TEXT("replicationCondition"), TEXT("None"));

    UBlueprint* Blueprint = LoadInventoryBlueprintOrError(
        Bridge, RequestId, RequestingSocket, BlueprintPath);
    if (!Blueprint) {
      return true;
    }

    // Resolve the condition up front: an unknown name used to replicate unconditionally (COND_None)
    // while the caller's value was echoed back as applied. "OwnerOnly" or "COND_OwnerOnly" both name it.
    const FString ConditionName = ReplicationCondition.StartsWith(TEXT("COND_")) ? ReplicationCondition : TEXT("COND_") + ReplicationCondition;
    const int64 Condition = StaticEnum<ELifetimeCondition>()->GetValueByNameString(ConditionName);
    if (Condition == INDEX_NONE) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
          FString::Printf(TEXT("Unknown replicationCondition '%s'; use an ELifetimeCondition name such as None, OwnerOnly, SkipOwner, SimulatedOnly or InitialOnly."), *ReplicationCondition),
          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    TArray<FString> ReplicatedVariables;

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
          // Only the replication flag and condition change: a RepNotify function already on the variable is
          // kept (this used to reset it to none, silently dropping the OnRep_ hook the caller had wired).
          Var.PropertyFlags |= CPF_Net;
          Var.ReplicationCondition = static_cast<ELifetimeCondition>(Condition);
        } else {
          Var.PropertyFlags &= ~CPF_Net;
          Var.ReplicationCondition = COND_None;
        }
        ReplicatedVariables.Add(Var.VarName.ToString());
      }
    }

    // Nothing to change used to answer success with modifiedVariables:[].
    if (ReplicatedVariables.Num() == 0) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
          TEXT("The Blueprint has none of the inventory variables this action replicates (InventorySlots, MaxSlots, CurrentWeight, MaxWeight); add them first with manage_blueprint add_variable."),
          TEXT("NOT_FOUND"));
      return true;
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if (GetJsonBoolField(Payload, TEXT("save"), true)) {
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

  return false;
}

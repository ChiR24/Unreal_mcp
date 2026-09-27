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
          Var.PropertyFlags |= CPF_Net;
          Var.RepNotifyFunc = NAME_None; // Can be set to a custom function name

          // "OwnerOnly" names COND_OwnerOnly; anything unknown replicates unconditionally.
          const int64 Condition = StaticEnum<ELifetimeCondition>()->GetValueByNameString(TEXT("COND_") + ReplicationCondition);
          Var.ReplicationCondition = Condition == INDEX_NONE ? COND_None : static_cast<ELifetimeCondition>(Condition);
        } else {
          Var.PropertyFlags &= ~CPF_Net;
          Var.ReplicationCondition = COND_None;
        }
        ReplicatedVariables.Add(Var.VarName.ToString());
      }
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

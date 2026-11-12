#include "Domains/Blueprint/McpBPActionContext.h"
#include "Domains/Blueprint/Components/McpBPScsOpFields.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpBlueprintHandlers {
bool PrepareModifyScsPayload(const FBlueprintActionContext &Context,
                             FModifyScsState &State) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (!LocalPayload->TryGetStringField(TEXT("blueprintPath"), State.BlueprintPath) ||
      State.BlueprintPath.TrimStartAndEnd().IsEmpty()) {
    Bridge.SendAutomationError(RequestingSocket, RequestId,
                               TEXT("blueprint_modify_scs requires a non-empty blueprintPath."),
                               TEXT("INVALID_BLUEPRINT"));
    return false;
  }
  // The published contract lists componentName/meshPath/materialPath/properties
  // on this action, but only operations[] was ever read -- so a caller following
  // the contract was refused for omitting an array the contract never mentions.
  // Promote a single-component payload into a one-op batch instead. The op is a
  // copy of the payload minus the batch-level keys: enumerating the fields to
  // forward is exactly how attachTo and applyAndSave got dropped before.
  if (!LocalPayload->HasField(TEXT("operations"))) {
    FString SingleComponent;
    if (LocalPayload->TryGetStringField(TEXT("componentName"), SingleComponent) &&
        !SingleComponent.TrimStartAndEnd().IsEmpty()) {
      TSharedPtr<FJsonObject> Op = MakeShared<FJsonObject>(*LocalPayload);
      static const TCHAR *const BatchKeys[] = {
          TEXT("blueprintPath"), TEXT("name"),   TEXT("blueprintCandidates"),
          TEXT("compile"),       TEXT("save"),   TEXT("applyAndSave"),
          TEXT("action"),        TEXT("edit")};
      for (const TCHAR *Key : BatchKeys) {
        Op->RemoveField(Key);
      }
      // location, rotation and scale stay on the op; ApplyModifyScsComponentOperation
      // folds them into `transform` for this op as for every other.
      // An add is a payload that names its class, read the way the add reads it.
      const bool bIsAdd = !ScsOpComponentClass(Op).IsEmpty();
      Op->SetStringField(TEXT("type"), bIsAdd ? TEXT("add_component")
                                              : TEXT("modify_component"));
      TArray<TSharedPtr<FJsonValue>> Synthesized;
      Synthesized.Add(MakeShared<FJsonValueObject>(Op));
      LocalPayload->SetArrayField(TEXT("operations"), Synthesized);
      State.LocalWarnings.Add(FString::Printf(
          TEXT("No operations array; ran the single-component payload as one "
               "%s op on '%s'."),
          bIsAdd ? TEXT("add_component") : TEXT("modify_component"),
          *SingleComponent));
    }
  }
  if (!LocalPayload->TryGetArrayField(TEXT("operations"), State.OperationsArray) ||
      State.OperationsArray == nullptr) {
    Bridge.SendAutomationError(RequestingSocket, RequestId,
        TEXT("blueprint_modify_scs requires an operations array, or a single "
             "componentName (plus componentClass to add) to run as one op."),
        TEXT("INVALID_OPERATIONS"));
    return false;
  }
  if (LocalPayload->HasField(TEXT("compile")) &&
      !LocalPayload->TryGetBoolField(TEXT("compile"), State.bCompile)) {
    Bridge.SendAutomationError(RequestingSocket, RequestId,
                               TEXT("compile must be a boolean."),
                               TEXT("INVALID_COMPILE_FLAG"));
    return false;
  }
  // An SCS edit that is not saved is lost on the next editor restart, so a
  // save is the default; save=false (or applyAndSave=false) opts out.
  State.bSave = true;
  if (LocalPayload->HasField(TEXT("save")) &&
      !LocalPayload->TryGetBoolField(TEXT("save"), State.bSave)) {
    Bridge.SendAutomationError(RequestingSocket, RequestId,
                               TEXT("save must be a boolean."),
                               TEXT("INVALID_SAVE_FLAG"));
    return false;
  }
  // The published contract calls this applyAndSave ("Whether to save the
  // Blueprint after applying SCS changes") and the gateway accepts it, but only
  // `save`/`compile` were ever read here - so a caller following the contract
  // got saved:false and lost the whole batch on the next editor restart.
  bool bApplyAndSave = false;
  if (LocalPayload->TryGetBoolField(TEXT("applyAndSave"), bApplyAndSave)) {
    State.bSave = bApplyAndSave;
    State.bCompile = State.bCompile || bApplyAndSave;
  }
  return true;
}

bool ResolveModifyScsTarget(const FBlueprintActionContext &Context,
                            FModifyScsState &State) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (FindBlueprintNormalizedPath(State.BlueprintPath, State.NormalizedBlueprintPath)) {
    UE_LOG(LogEngineWireSubsystem, Log,
           TEXT("blueprint_modify_scs: resolved explicit path %s -> %s"),
           *State.BlueprintPath, *State.NormalizedBlueprintPath);
    return true;
  }
  TSharedPtr<FJsonObject> ErrPayload = McpHandlerUtils::CreateResultObject();
  ErrPayload->SetArrayField(TEXT("triedCandidates"), McpHandlerUtils::ToJsonStringArray({State.BlueprintPath}));
  Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
      FString::Printf(TEXT("Blueprint not found for path %s"), *State.BlueprintPath),
      ErrPayload, TEXT("BLUEPRINT_NOT_FOUND"));
  return false;
}

bool RequireModifyScsOperations(const FBlueprintActionContext &Context,
                                FModifyScsState &State) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (State.OperationsArray->Num() == 0) {
    TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
    ResultPayload->SetStringField(TEXT("blueprintPath"), State.NormalizedBlueprintPath);
    ResultPayload->SetArrayField(TEXT("operations"), TArray<TSharedPtr<FJsonValue>>());
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        TEXT("No SCS operations supplied."), ResultPayload, FString());
    return false;
  }
  return true;
}
} // namespace McpBlueprintHandlers

#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool PrepareModifyScsPayload(const FBlueprintActionContext &Context,
                             FModifyScsState &State) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (!LocalPayload.IsValid()) {
    Bridge.SendAutomationError(RequestingSocket, RequestId,
                               TEXT("blueprint_modify_scs payload missing."),
                               TEXT("INVALID_PAYLOAD"));
    return false;
  }
  if (!LocalPayload->TryGetStringField(TEXT("blueprintPath"), State.BlueprintPath) ||
      State.BlueprintPath.TrimStartAndEnd().IsEmpty()) {
    if (!LocalPayload->TryGetStringField(TEXT("name"), State.BlueprintPath) ||
        State.BlueprintPath.TrimStartAndEnd().IsEmpty()) {
      const TArray<TSharedPtr<FJsonValue>> *CandidateArray = nullptr;
      if (!LocalPayload->TryGetArrayField(TEXT("blueprintCandidates"), CandidateArray) ||
          CandidateArray == nullptr || CandidateArray->Num() == 0) {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("blueprint_modify_scs requires a non-empty blueprintPath, name, or blueprintCandidates."),
            TEXT("INVALID_BLUEPRINT"));
        return false;
      }
      for (const TSharedPtr<FJsonValue> &Val : *CandidateArray) {
        if (Val.IsValid()) {
          const FString Candidate = Val->AsString();
          if (!Candidate.TrimStartAndEnd().IsEmpty()) {
            State.CandidatePaths.Add(Candidate);
          }
        }
      }
      if (State.CandidatePaths.Num() == 0) {
        Bridge.SendAutomationError(RequestingSocket, RequestId,
            TEXT("blueprint_modify_scs blueprintCandidates array provided but contains no valid strings."),
            TEXT("INVALID_BLUEPRINT_CANDIDATES"));
        return false;
      }
    }
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
      const bool bIsAdd = Op->HasField(TEXT("componentClass")) ||
                          Op->HasField(TEXT("componentType"));
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
  if (!State.BlueprintPath.IsEmpty()) {
    State.TriedCandidates.Add(State.BlueprintPath);
    if (FindBlueprintNormalizedPath(State.BlueprintPath, State.NormalizedBlueprintPath)) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
             TEXT("blueprint_modify_scs: resolved explicit path %s -> %s"),
             *State.BlueprintPath, *State.NormalizedBlueprintPath);
    } else {
      State.LoadError = FString::Printf(TEXT("Blueprint not found for path %s"),
                                        *State.BlueprintPath);
    }
  }
  if (State.NormalizedBlueprintPath.IsEmpty() && State.CandidatePaths.Num() > 0) {
    for (const FString &Candidate : State.CandidatePaths) {
      State.TriedCandidates.Add(Candidate);
      FString CandidateNormalized;
      if (FindBlueprintNormalizedPath(Candidate, CandidateNormalized)) {
        State.NormalizedBlueprintPath = CandidateNormalized;
        State.LoadError.Empty();
        UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
               TEXT("blueprint_modify_scs: resolved candidate %s -> %s"),
               *Candidate, *CandidateNormalized);
        break;
      }
      State.LoadError = FString::Printf(TEXT("Candidate not found: %s"), *Candidate);
    }
  }
  if (!State.NormalizedBlueprintPath.IsEmpty()) {
    return true;
  }
  TSharedPtr<FJsonObject> ErrPayload = McpHandlerUtils::CreateResultObject();
  if (State.TriedCandidates.Num() > 0) {
    TArray<TSharedPtr<FJsonValue>> TriedValues;
    for (const FString &C : State.TriedCandidates) {
      TriedValues.Add(MakeShared<FJsonValueString>(C));
    }
    ErrPayload->SetArrayField(TEXT("triedCandidates"), TriedValues);
  }
  Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
      State.LoadError.IsEmpty() ? TEXT("Blueprint not found") : State.LoadError,
      ErrPayload, TEXT("BLUEPRINT_NOT_FOUND"));
  return false;
}

bool AcquireModifyScsBusy(const FBlueprintActionContext &Context,
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
  if (GBlueprintBusySet.Contains(State.NormalizedBlueprintPath)) {
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
        FString::Printf(TEXT("Blueprint %s is busy with another modification."),
                        *State.NormalizedBlueprintPath),
        nullptr, TEXT("BLUEPRINT_BUSY"));
    return false;
  }
  GBlueprintBusySet.Add(State.NormalizedBlueprintPath);
  Bridge.CurrentBusyBlueprintKey = State.NormalizedBlueprintPath;
  Bridge.bCurrentBlueprintBusyMarked = true;
  Bridge.bCurrentBlueprintBusyScheduled = false;
  return true;
}
#endif
} // namespace McpBlueprintHandlers

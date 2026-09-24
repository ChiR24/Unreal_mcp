#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintDiagnostics.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersScsPropagate.h"
#include "Engine/Blueprint.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
void FinalizeModifyScsResponse(const FBlueprintActionContext &Context,
                               FModifyScsState &State,
                               UBlueprint *LocalBP) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  // A failed operation used to vanish into operations[]: a batch whose six
  // reparents were all rejected still answered success, "Processed 12 SCS
  // operation(s).", and no warnings. Name every failure at the top, and fail
  // outright when nothing in the batch applied.
  int32 Failed = 0;
  for (const TSharedPtr<FJsonValue> &Summary : State.FinalSummaries) {
    const TSharedPtr<FJsonObject> *Op = nullptr;
    bool bSucceeded = true;
    if (!Summary.IsValid() || !Summary->TryGetObject(Op) || !Op ||
        !(*Op)->TryGetBoolField(TEXT("success"), bSucceeded) || bSucceeded) {
      continue;
    }
    ++Failed;
    FString Reason = GetJsonStringField(*Op, TEXT("warning"));
    if (Reason.IsEmpty()) {
      Reason = GetJsonStringField(*Op, TEXT("error"));
    }
    State.LocalWarnings.Add(FString::Printf(
        TEXT("operation %d (%s) failed: %s"),
        static_cast<int32>((*Op)->GetNumberField(TEXT("index"))),
        *GetJsonStringField(*Op, TEXT("type")),
        Reason.IsEmpty() ? TEXT("no reason given") : *Reason));
  }
  // A property the template refused is named even when the rest of its
  // operation applied, so a partial prefab is never reported as a clean one.
  for (const TSharedPtr<FJsonValue> &Summary : State.FinalSummaries) {
    const TSharedPtr<FJsonObject> *Op = nullptr;
    const TArray<TSharedPtr<FJsonValue>> *Rejected = nullptr;
    if (!Summary.IsValid() || !Summary->TryGetObject(Op) || !Op ||
        !(*Op)->TryGetArrayField(TEXT("rejectedProperties"), Rejected)) {
      continue;
    }
    for (const TSharedPtr<FJsonValue> &Entry : *Rejected) {
      State.LocalWarnings.Add(FString::Printf(TEXT("operation %d (%s) did not apply %s"),
          static_cast<int32>((*Op)->GetNumberField(TEXT("index"))),
          *GetJsonStringField(*Op, TEXT("componentName")), *Entry->AsString()));
    }
  }
  State.bOk = State.FinalSummaries.Num() > Failed;
  State.CompletionResult->SetArrayField(TEXT("operations"), State.FinalSummaries);
  // `compiled` used to echo the REQUEST flag, so a batch that left the
  // blueprint broken still answered compiled:true and the breakage stayed
  // invisible until play. Compile first, report what the compiler said, and do
  // not write a blueprint to disk that just failed to compile.
  bool bCompileOk = false;
  TSharedPtr<FJsonObject> CompileInfo = McpHandlerUtils::CreateResultObject();
  if (State.bCompile && LocalBP) {
    FString CompileError;
    bCompileOk =
        McpCompileBlueprintWithDiagnostics(LocalBP, CompileInfo, CompileError, 6);
    if (!bCompileOk) {
      State.LocalWarnings.Add(FString::Printf(
          TEXT("Blueprint does NOT compile after these operations: %s"),
          CompileError.IsEmpty() ? TEXT("no compiler message") : *CompileError));
    }
  }
  TArray<FString> RepropagateMissed;
  const int32 Repropagated = McpScsPropagate::RepropagatePending(&RepropagateMissed);
  if (Repropagated > 0) {
    State.CompletionResult->SetNumberField(TEXT("instancesRepropagated"), Repropagated);
  }
  // The final say on every placed instance: one still holding the old value
  // after this last push is named, never counted as updated.
  for (const FString &Missed : RepropagateMissed) {
    State.LocalWarnings.Add(FString::Printf(
        TEXT("placed instance kept its old value: %s"), *Missed));
  }
  if (State.bSave && LocalBP && !(State.bCompile && !bCompileOk)) {
    State.bSaveResult = SaveLoadedAssetThrottled(LocalBP);
    if (!State.bSaveResult) {
      State.LocalWarnings.Add(TEXT("Blueprint failed to save during apply; check output log."));
    }
  }
  State.CompletionResult->SetStringField(TEXT("blueprintPath"), State.NormalizedBlueprintPath);
  State.CompletionResult->SetBoolField(TEXT("compiled"), bCompileOk);
  State.CompletionResult->SetBoolField(TEXT("saved"), State.bSave && State.bSaveResult);
  TArray<TSharedPtr<FJsonValue>> WarningValues;
  for (const FString &Warning : State.LocalWarnings) {
    WarningValues.Add(MakeShared<FJsonValueString>(Warning));
  }
  if (WarningValues.Num() > 0) {
    State.CompletionResult->SetArrayField(TEXT("warnings"), WarningValues);
  }
  TSharedPtr<FJsonObject> Notify = McpHandlerUtils::CreateResultObject();
  Notify->SetStringField(TEXT("type"), TEXT("automation_event"));
  Notify->SetStringField(TEXT("event"), TEXT("modify_scs_completed"));
  Notify->SetStringField(TEXT("requestId"), RequestId);
  Notify->SetObjectField(TEXT("result"), State.CompletionResult);
  Bridge.BroadcastAutomationEvent(Notify, RequestingSocket);
  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("blueprintPath"), State.NormalizedBlueprintPath);
  ResultPayload->SetArrayField(TEXT("operations"), State.FinalSummaries);
  ResultPayload->SetBoolField(TEXT("compiled"), bCompileOk);
  ResultPayload->SetStringField(
      TEXT("compilerStatus"),
      LocalBP ? McpBlueprintStatusName(LocalBP->Status) : TEXT("Unknown"));
  const TArray<TSharedPtr<FJsonValue>> *ScsDiagnostics = nullptr;
  if (CompileInfo->TryGetArrayField(TEXT("diagnostics"), ScsDiagnostics)) {
    ResultPayload->SetArrayField(TEXT("diagnostics"), *ScsDiagnostics);
  }
  ResultPayload->SetBoolField(TEXT("saved"), State.bSave && State.bSaveResult);
  if (Repropagated > 0) {
    ResultPayload->SetNumberField(TEXT("instancesRepropagated"), Repropagated);
  }
  if (WarningValues.Num() > 0) {
    ResultPayload->SetArrayField(TEXT("warnings"), WarningValues);
  }
  const FString Message = Failed > 0
      ? FString::Printf(TEXT("Processed %d SCS operation(s); %d failed (see warnings)."),
                        State.FinalSummaries.Num(), Failed)
      : FString::Printf(TEXT("Processed %d SCS operation(s)."), State.FinalSummaries.Num());
  Bridge.SendAutomationResponse(RequestingSocket, RequestId, State.bOk, Message,
      ResultPayload, State.bOk ? FString() :
          (State.CompletionResult->HasField(TEXT("error")) ?
               GetJsonStringField(State.CompletionResult, TEXT("error")) :
               TEXT("SCS_OPERATION_FAILED")));
  if (!Bridge.CurrentBusyBlueprintKey.IsEmpty() &&
      GBlueprintBusySet.Contains(Bridge.CurrentBusyBlueprintKey)) {
    GBlueprintBusySet.Remove(Bridge.CurrentBusyBlueprintKey);
  }
  Bridge.bCurrentBlueprintBusyMarked = false;
  Bridge.bCurrentBlueprintBusyScheduled = false;
  Bridge.CurrentBusyBlueprintKey.Empty();
}
#endif
} // namespace McpBlueprintHandlers

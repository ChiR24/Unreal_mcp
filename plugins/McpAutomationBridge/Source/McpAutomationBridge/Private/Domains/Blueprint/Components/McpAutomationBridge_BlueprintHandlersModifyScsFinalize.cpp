#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintDiagnostics.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
void FinalizeModifyScsResponse(const FBlueprintActionContext &Context,
                               FModifyScsState &State,
                               UBlueprint *LocalBP) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  State.bOk = State.FinalSummaries.Num() > 0;
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
  if (WarningValues.Num() > 0) {
    ResultPayload->SetArrayField(TEXT("warnings"), WarningValues);
  }
  const FString Message = FString::Printf(TEXT("Processed %d SCS operation(s)."),
                                          State.FinalSummaries.Num());
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

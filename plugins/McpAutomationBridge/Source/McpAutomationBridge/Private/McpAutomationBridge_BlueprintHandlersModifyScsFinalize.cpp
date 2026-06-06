#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "McpAutomationBridgeHelpersJsonFields.h"
#include "McpConnectionManager.h"
#include "McpHandlerUtils.h"

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
  if (State.bSave && LocalBP) {
    State.bSaveResult = SaveLoadedAssetThrottled(LocalBP);
    if (!State.bSaveResult) {
      State.LocalWarnings.Add(TEXT("Blueprint failed to save during apply; check output log."));
    }
  }
  if (State.bCompile && LocalBP) {
    McpSafeCompileBlueprint(LocalBP);
  }
  State.CompletionResult->SetStringField(TEXT("blueprintPath"), State.NormalizedBlueprintPath);
  State.CompletionResult->SetBoolField(TEXT("compiled"), State.bCompile);
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
  if (Bridge.ConnectionManager.IsValid()) {
    Bridge.ConnectionManager->SendControlMessage(Notify);
  }
  TSharedPtr<FJsonObject> ResultPayload = McpHandlerUtils::CreateResultObject();
  ResultPayload->SetStringField(TEXT("blueprintPath"), State.NormalizedBlueprintPath);
  ResultPayload->SetArrayField(TEXT("operations"), State.FinalSummaries);
  ResultPayload->SetBoolField(TEXT("compiled"), State.bCompile);
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

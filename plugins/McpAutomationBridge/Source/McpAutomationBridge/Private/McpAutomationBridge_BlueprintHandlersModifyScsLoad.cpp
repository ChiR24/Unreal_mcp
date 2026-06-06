#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool ValidateModifyScsOperations(const FBlueprintActionContext &Context,
                                 FModifyScsState &State) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  State.DeferredOps = *State.OperationsArray;
  for (int32 Index = 0; Index < State.DeferredOps.Num(); ++Index) {
    const TSharedPtr<FJsonValue> &OperationValue = State.DeferredOps[Index];
    if (!OperationValue.IsValid() || OperationValue->Type != EJson::Object) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
          FString::Printf(TEXT("Operation at index %d is not an object."), Index),
          TEXT("INVALID_OPERATION_PAYLOAD"));
      return false;
    }
    const TSharedPtr<FJsonObject> OperationObject = OperationValue->AsObject();
    FString OperationType;
    if (!OperationObject->TryGetStringField(TEXT("type"), OperationType) ||
        OperationType.TrimStartAndEnd().IsEmpty()) {
      Bridge.SendAutomationError(RequestingSocket, RequestId,
          FString::Printf(TEXT("Operation at index %d missing type."), Index),
          TEXT("INVALID_OPERATION_TYPE"));
      return false;
    }
  }
  Bridge.bCurrentBlueprintBusyScheduled = true;
  return true;
}

bool LoadModifyScsBlueprint(const FBlueprintActionContext &Context,
                            FModifyScsState &State, UBlueprint *&LocalBP,
                            USimpleConstructionScript *&LocalSCS) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  State.CompletionResult = McpHandlerUtils::CreateResultObject();
  FString LocalNormalized;
  FString LocalLoadError;
  LocalBP = LoadBlueprintAsset(State.NormalizedBlueprintPath, LocalNormalized, LocalLoadError);
  if (!LocalBP) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("SCS application failed to load blueprint %s: %s"),
           *State.NormalizedBlueprintPath, *LocalLoadError);
    State.CompletionResult->SetStringField(TEXT("error"), LocalLoadError);
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, LocalLoadError,
                                  State.CompletionResult, TEXT("BLUEPRINT_NOT_FOUND"));
  } else {
    LocalSCS = LocalBP->SimpleConstructionScript;
    if (LocalSCS) {
      return true;
    }
    UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
           TEXT("SCS unavailable for blueprint %s"), *State.NormalizedBlueprintPath);
    State.CompletionResult->SetStringField(TEXT("error"), TEXT("SCS_UNAVAILABLE"));
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                                  TEXT("SCS_UNAVAILABLE"), State.CompletionResult,
                                  TEXT("SCS_UNAVAILABLE"));
  }
  if (!Bridge.CurrentBusyBlueprintKey.IsEmpty() &&
      GBlueprintBusySet.Contains(Bridge.CurrentBusyBlueprintKey)) {
    GBlueprintBusySet.Remove(Bridge.CurrentBusyBlueprintKey);
  }
  Bridge.bCurrentBlueprintBusyMarked = false;
  Bridge.bCurrentBlueprintBusyScheduled = false;
  Bridge.CurrentBusyBlueprintKey.Empty();
  return false;
}
#endif
} // namespace McpBlueprintHandlers

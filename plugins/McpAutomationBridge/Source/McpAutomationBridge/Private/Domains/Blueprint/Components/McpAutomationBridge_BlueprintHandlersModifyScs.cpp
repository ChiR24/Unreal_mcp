#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR
#include "Domains/Blueprint/Components/McpAutomationBridge_BlueprintHandlersScsPropagate.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
bool HandleBlueprintModifyScs(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (!(ActionMatchesPattern(TEXT("blueprint_modify_scs")) ||
        ActionMatchesPattern(TEXT("modify_scs")) ||
        ActionMatchesPattern(TEXT("modifyscs")) ||
        AlphaNumLower.Contains(TEXT("blueprintmodifyscs")) ||
        AlphaNumLower.Contains(TEXT("modifyscs")))) {
    return false;
  }

  const double HandlerStartTimeSec = FPlatformTime::Seconds();
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("blueprint_modify_scs handler start (RequestId=%s)"), *RequestId);
  FModifyScsState State;
  // Installed BEFORE the guard chain, not after it. AcquireModifyScsBusy marks
  // the Blueprint busy and ValidateModifyScsOperations runs after it, so an
  // operation missing `type` short-circuited straight to `return true` while
  // the scope guard below had not been registered yet -- the busy entry was
  // never removed and that Blueprint refused every later modify_scs until the
  // editor restarted. One malformed op permanently bricked the asset. Nothing
  // is marked until Acquire succeeds, so hoisting this is a no-op otherwise.
  ON_SCOPE_EXIT {
    if (Bridge.bCurrentBlueprintBusyMarked && !Bridge.bCurrentBlueprintBusyScheduled) {
      GBlueprintBusySet.Remove(Bridge.CurrentBusyBlueprintKey);
      Bridge.bCurrentBlueprintBusyMarked = false;
      Bridge.CurrentBusyBlueprintKey.Empty();
    }
  };

  if (!PrepareModifyScsPayload(Context, State) ||
      !ResolveModifyScsTarget(Context, State) ||
      !AcquireModifyScsBusy(Context, State) ||
      !ValidateModifyScsOperations(Context, State)) {
    return true;
  }

  UBlueprint *LocalBP = nullptr;
  USimpleConstructionScript *LocalSCS = nullptr;
  if (!LoadModifyScsBlueprint(Context, State, LocalBP, LocalSCS)) {
    return true;
  }

  LocalBP->Modify();
  LocalSCS->Modify();
  McpScsPropagate::Pending().Reset();
  for (int32 Index = 0; Index < State.DeferredOps.Num(); ++Index) {
    const double OpStart = FPlatformTime::Seconds();
    const TSharedPtr<FJsonValue> &Value = State.DeferredOps[Index];
    if (!Value.IsValid() || Value->Type != EJson::Object) {
      continue;
    }
    const TSharedPtr<FJsonObject> Op = Value->AsObject();
    FString OpType;
    Op->TryGetStringField(TEXT("type"), OpType);
    const FString NormalizedType = OpType.ToLower();
    TSharedPtr<FJsonObject> OpSummary = McpHandlerUtils::CreateResultObject();
    OpSummary->SetNumberField(TEXT("index"), Index);
    OpSummary->SetStringField(TEXT("type"), NormalizedType);
    ApplyModifyScsOperation(LocalBP, LocalSCS, NormalizedType, Op, OpSummary);
    const double OpElapsedMs = (FPlatformTime::Seconds() - OpStart) * 1000.0;
    OpSummary->SetNumberField(TEXT("durationMs"), OpElapsedMs);
    State.FinalSummaries.Add(MakeShared<FJsonValueObject>(OpSummary));
  }

  FinalizeModifyScsResponse(Context, State, LocalBP);
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("blueprint_modify_scs completed in %.2f ms"),
         (FPlatformTime::Seconds() - HandlerStartTimeSec) * 1000.0);
  return true;
}
#endif
} // namespace McpBlueprintHandlers

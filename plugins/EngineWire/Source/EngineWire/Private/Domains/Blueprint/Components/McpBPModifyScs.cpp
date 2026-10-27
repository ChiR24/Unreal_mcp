#include "Domains/Blueprint/McpBPActionContext.h"
#include "Core/Module/EngineWireGlobals.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"

#include "Domains/Blueprint/Components/McpBPScsPropagate.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"

namespace McpBlueprintHandlers {
bool HandleBlueprintModifyScs(const FBlueprintActionContext &Context) {
  MCP_BLUEPRINT_ACTION_LOCALS(Context);
  if (!(ActionMatchesPattern(TEXT("modify_scs")))) {
    return false;
  }

  const double HandlerStartTimeSec = FPlatformTime::Seconds();
  UE_LOG(LogEngineWireSubsystem, Verbose,
         TEXT("blueprint_modify_scs handler start (RequestId=%s)"), *RequestId);
  FModifyScsState State;
  if (!PrepareModifyScsPayload(Context, State) ||
      !ResolveModifyScsTarget(Context, State) ||
      !RequireModifyScsOperations(Context, State) ||
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
    // An op's durationMs (milliseconds per edit) was log detail in every reply; the handler's time stays in the log.
    ApplyModifyScsOperation(LocalBP, LocalSCS, NormalizedType, Op, OpSummary);
    State.FinalSummaries.Add(MakeShared<FJsonValueObject>(OpSummary));
  }

  FinalizeModifyScsResponse(Context, State, LocalBP);
  UE_LOG(LogEngineWireSubsystem, Verbose,
         TEXT("blueprint_modify_scs completed in %.2f ms"),
         (FPlatformTime::Seconds() - HandlerStartTimeSec) * 1000.0);
  return true;
}
} // namespace McpBlueprintHandlers

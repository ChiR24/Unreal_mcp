#include "McpAutomationBridge_BlueprintActionContext.h"
#include "McpAutomationBridgeHelpersBlueprintPaths.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#endif

namespace McpBlueprintHandlers {
#if WITH_EDITOR
namespace {
FString CleanBlueprintAction(const FString &Action) {
  FString CleanAction;
  CleanAction.Reserve(Action.Len());
  for (int32 Idx = 0; Idx < Action.Len(); ++Idx) {
    const TCHAR C = Action[Idx];
    if (C < 32 || C == 0x200B || C == 0xFEFF || C == 0x200C || C == 0x200D) {
      continue;
    }
    CleanAction.AppendChar(C);
  }
  CleanAction.TrimStartAndEndInline();
  return CleanAction;
}

FString CompactActionKey(const FString &Action) {
  FString AlphaNumLower;
  AlphaNumLower.Reserve(Action.Len());
  for (int32 i = 0; i < Action.Len(); ++i) {
    const TCHAR C = Action[i];
    if (FChar::IsAlnum(C)) {
      AlphaNumLower.AppendChar(FChar::ToLower(C));
    }
  }
  return AlphaNumLower;
}

void ExtractNestedManageAction(FBlueprintActionContext &Context,
                               FString &LowerNormalized) {
  if (!(LowerNormalized.StartsWith(TEXT("manage_blueprint")) ||
        LowerNormalized.StartsWith(TEXT("manageblueprint"))) ||
      !Context.LocalPayload.IsValid()) {
    return;
  }
  FString Nested;
  if (!Context.LocalPayload->TryGetStringField(TEXT("action"), Nested) ||
      Nested.TrimStartAndEnd().IsEmpty()) {
    return;
  }
  Context.CleanAction = CleanBlueprintAction(Nested);
  if (Context.CleanAction.IsEmpty()) {
    return;
  }
  Context.Lower = Context.CleanAction.ToLower();
  LowerNormalized = Context.Lower;
  LowerNormalized.ReplaceInline(TEXT("-"), TEXT("_"));
  LowerNormalized.ReplaceInline(TEXT(" "), TEXT("_"));
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("manage_blueprint nested action detected: %s -> %s"),
         *Context.Action, *Context.CleanAction);
}
} // namespace

FBlueprintActionContext BuildBlueprintActionContext(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &Action, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FBlueprintActionContext Context{Bridge, RequestId, Action};
  Context.RequestingSocket = RequestingSocket;
  Context.LocalPayload = Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  Context.CleanAction = CleanBlueprintAction(Action);
  Context.Lower = Context.CleanAction.ToLower();
  FString LowerNormalized = Context.Lower;
  LowerNormalized.ReplaceInline(TEXT("-"), TEXT("_"));
  LowerNormalized.ReplaceInline(TEXT(" "), TEXT("_"));
  const bool bManageWrapperHint =
      LowerNormalized.StartsWith(TEXT("manage_blueprint")) ||
      LowerNormalized.StartsWith(TEXT("manageblueprint"));
  ExtractNestedManageAction(Context, LowerNormalized);
  if (Context.Lower.StartsWith(TEXT("manage_blueprint"))) {
    ExtractNestedManageAction(Context, LowerNormalized);
  }
  Context.AlphaNumLower = CompactActionKey(Context.CleanAction);
  Context.bLooksBlueprint = LowerNormalized.StartsWith(TEXT("blueprint_")) ||
      LowerNormalized.StartsWith(TEXT("manage_blueprint")) ||
      LowerNormalized.StartsWith(TEXT("manageblueprint")) || bManageWrapperHint ||
      LowerNormalized.Contains(TEXT("scs_component")) ||
      LowerNormalized.Contains(TEXT("_scs")) ||
      Context.AlphaNumLower.Contains(TEXT("blueprint")) ||
      Context.AlphaNumLower.Contains(TEXT("scs"));
  return Context;
}

FBlueprintActionContext BuildScsActionContext(
    UMcpAutomationBridgeSubsystem &Bridge, const FString &RequestId,
    const FString &Action, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FBlueprintActionContext Context{Bridge, RequestId, Action};
  Context.RequestingSocket = RequestingSocket;
  Context.LocalPayload = Payload;
  Context.CleanAction = Action;
  Context.CleanAction.TrimStartAndEndInline();
  Context.Lower = Context.CleanAction.ToLower();
  Context.AlphaNumLower = CompactActionKey(Context.CleanAction);
  Context.bLooksBlueprint = true;
  return Context;
}

bool ActionMatchesPattern(const FBlueprintActionContext &Context,
                          const TCHAR *Pattern) {
  const FString PatternStr = FString(Pattern).ToLower();
  FString PatternAlpha;
  PatternAlpha.Reserve(PatternStr.Len());
  for (int32 i = 0; i < PatternStr.Len(); ++i) {
    const TCHAR C = PatternStr[i];
    if (FChar::IsAlnum(C)) {
      PatternAlpha.AppendChar(C);
    }
  }
  const bool bExactOrContains = Context.Lower.Equals(PatternStr);
  const bool bAlphaMatch = !Context.AlphaNumLower.IsEmpty() &&
      !PatternAlpha.IsEmpty() && Context.AlphaNumLower.Equals(PatternAlpha);
  UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose,
         TEXT("ActionMatchesPattern check: pattern='%s' patternAlpha='%s' lower='%s' alpha='%s' matched=%s"),
         *PatternStr, *PatternAlpha, *Context.Lower, *Context.AlphaNumLower,
         (bExactOrContains || bAlphaMatch) ? TEXT("true") : TEXT("false"));
  return bExactOrContains || bAlphaMatch;
}

bool ActionMatchesPatternImpl(const FString &Lower,
                              const FString &AlphaNumLower,
                              const TCHAR *Pattern) {
  const FString PatternStr = FString(Pattern).ToLower();
  FString PatternAlpha;
  PatternAlpha.Reserve(PatternStr.Len());
  for (int32 i = 0; i < PatternStr.Len(); ++i) {
    const TCHAR C = PatternStr[i];
    if (FChar::IsAlnum(C)) {
      PatternAlpha.AppendChar(C);
    }
  }
  return Lower.Equals(PatternStr) || Lower.Contains(PatternStr) ||
      (!AlphaNumLower.IsEmpty() && !PatternAlpha.IsEmpty() &&
       AlphaNumLower.Contains(PatternAlpha));
}

void DiagnosticPatternChecks(const FBlueprintActionContext &Context) {
  const TCHAR *Patterns[] = {TEXT("blueprint_add_variable"), TEXT("add_variable"),
      TEXT("addvariable"), TEXT("blueprint_add_event"), TEXT("add_event"),
      TEXT("blueprint_add_function"), TEXT("add_function"),
      TEXT("blueprint_modify_scs"), TEXT("modify_scs"),
      TEXT("blueprint_set_default"), TEXT("set_default"),
      TEXT("blueprint_set_variable_metadata"), TEXT("set_variable_metadata"),
      TEXT("blueprint_compile"), TEXT("blueprint_probe_subobject_handle"),
      TEXT("blueprint_exists"), TEXT("blueprint_get"), TEXT("blueprint_create")};
  for (const TCHAR *P : Patterns) {
    const bool bMatch = ActionMatchesPatternImpl(Context.Lower, Context.AlphaNumLower, P);
    UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose,
           TEXT("Diagnostic pattern check: Action=%s Pattern=%s Matched=%s"),
           *Context.CleanAction, P, bMatch ? TEXT("true") : TEXT("false"));
  }
}

FString ResolveBlueprintRequestedPath(const TSharedPtr<FJsonObject> &LocalPayload) {
  if (!LocalPayload.IsValid()) {
    return FString();
  }
  FString Req;
  const TCHAR *Keys[] = {TEXT("requestedPath"), TEXT("name"), TEXT("blueprintPath")};
  for (const TCHAR *Key : Keys) {
    if (LocalPayload->TryGetStringField(Key, Req) && !Req.TrimStartAndEnd().IsEmpty()) {
      FString Norm;
      return FindBlueprintNormalizedPath(Req, Norm) && !Norm.TrimStartAndEnd().IsEmpty() ? Norm : Req;
    }
  }
  const TArray<TSharedPtr<FJsonValue>> *CandidateArray = nullptr;
  const TCHAR *ArrayKeys[] = {TEXT("blueprintCandidates"), TEXT("candidates")};
  for (const TCHAR *Key : ArrayKeys) {
    if (LocalPayload->TryGetArrayField(Key, CandidateArray) && CandidateArray && CandidateArray->Num() > 0) {
      for (const TSharedPtr<FJsonValue> &V : *CandidateArray) {
        if (!V.IsValid() || V->Type != EJson::String) {
          continue;
        }
        FString Candidate = V->AsString();
        if (Candidate.TrimStartAndEnd().IsEmpty()) {
          continue;
        }
        FString Norm;
        return FindBlueprintNormalizedPath(Candidate, Norm) && !Norm.TrimStartAndEnd().IsEmpty() ? Norm : Candidate;
      }
    }
  }
  return FString();
}

UBlueprint *ResolveScsBlueprint(const TSharedPtr<FJsonObject> &Payload) {
  FString BlueprintPath;
  if (Payload.IsValid() && (Payload->TryGetStringField(TEXT("name"), BlueprintPath) ||
      Payload->TryGetStringField(TEXT("blueprintPath"), BlueprintPath)) && !BlueprintPath.IsEmpty()) {
    return LoadObject<UBlueprint>(nullptr, *BlueprintPath);
  }
  const TArray<TSharedPtr<FJsonValue>> *Candidates = nullptr;
  if (Payload.IsValid() && Payload->TryGetArrayField(TEXT("blueprintCandidates"), Candidates) &&
      Candidates && Candidates->Num() > 0) {
    for (const TSharedPtr<FJsonValue> &Candidate : *Candidates) {
      if (Candidate.IsValid() && Candidate->Type == EJson::String) {
        FString CandidatePath = Candidate->AsString();
        if (!CandidatePath.IsEmpty()) {
          if (UBlueprint *BP = LoadObject<UBlueprint>(nullptr, *CandidatePath)) {
            return BP;
          }
        }
      }
    }
  }
  return nullptr;
}
#endif
} // namespace McpBlueprintHandlers

#include "Domains/Blueprint/McpBPActionContext.h"
#include "Core/Routing/McpConsolidatedActionRouting.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPPaths.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPAssetLoad.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"

#include "Engine/Blueprint.h"

namespace McpBlueprintHandlers {
namespace {
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

// manage_blueprint carries the action to run as its sub-action, read as every dispatcher reads it.
void ExtractNestedManageAction(FBlueprintActionContext &Context) {
  if (Context.Lower != TEXT("manage_blueprint")) {
    return;
  }
  const FString Nested = McpConsolidatedActions::GetPayloadSubAction(Context.LocalPayload);
  if (Nested.IsEmpty()) {
    return;
  }
  Context.CleanAction = Nested;
  Context.Lower = Nested;
  UE_LOG(LogEngineWireSubsystem, Verbose,
         TEXT("manage_blueprint nested action detected: %s -> %s"),
         *Context.Action, *Context.CleanAction);
}
} // namespace

FBlueprintActionContext BuildBlueprintActionContext(
    UEngineWireSubsystem &Bridge, const FString &RequestId,
    const FString &Action, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FBlueprintActionContext Context{Bridge, RequestId, Action};
  Context.RequestingSocket = RequestingSocket;
  Context.LocalPayload = Payload.IsValid() ? Payload : McpHandlerUtils::CreateResultObject();
  Context.CleanAction = Action;
  Context.Lower = Action.ToLower();
  ExtractNestedManageAction(Context);
  Context.AlphaNumLower = CompactActionKey(Context.CleanAction);
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
  UE_LOG(LogEngineWireSubsystem, VeryVerbose,
         TEXT("ActionMatchesPattern check: pattern='%s' patternAlpha='%s' lower='%s' alpha='%s' matched=%s"),
         *PatternStr, *PatternAlpha, *Context.Lower, *Context.AlphaNumLower,
         (bExactOrContains || bAlphaMatch) ? TEXT("true") : TEXT("false"));
  return bExactOrContains || bAlphaMatch;
}
FString ResolveBlueprintRequestedPath(const TSharedPtr<FJsonObject> &LocalPayload) {
  if (!LocalPayload.IsValid()) {
    return FString();
  }
  FString Req;
  // Order matters: `blueprintPath` must win over `name`. Several actions
  // (e.g. add_event) carry a `name` field that is NOT the blueprint path
  // (it is the event/function name), so resolving `name` first would misload
  // the blueprint. `name` is the last resort: create and ensure_exists declare it.
  const TCHAR *Keys[] = {TEXT("blueprintPath"), TEXT("name")};
  for (const TCHAR *Key : Keys) {
    if (LocalPayload->TryGetStringField(Key, Req) && !Req.TrimStartAndEnd().IsEmpty()) {
      FString Norm;
      return FindBlueprintNormalizedPath(Req, Norm) && !Norm.TrimStartAndEnd().IsEmpty() ? Norm : Req;
    }
  }
  return FString();
}

FString RequireBlueprintPath(const FBlueprintActionContext &Context, const TCHAR *Action) {
  const FString Path = ResolveBlueprintRequestedPath(Context.LocalPayload);
  if (Path.IsEmpty()) {
    Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
        FString::Printf(TEXT("%s requires a blueprint path."), Action), TEXT("INVALID_BLUEPRINT_PATH"));
  }
  return Path;
}

UBlueprint *LoadBlueprintOrReply(const FBlueprintActionContext &Context, const FString &Path, FString &OutNormalized) {
  FString LoadErr;
  UBlueprint *Blueprint = LoadBlueprintAsset(Path, OutNormalized, LoadErr);
  if (!Blueprint) {
    TSharedPtr<FJsonObject> Err = McpHandlerUtils::CreateResultObject();
    if (!LoadErr.IsEmpty()) {
      Err->SetStringField(TEXT("error"), LoadErr);
    }
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, false, TEXT("Failed to load blueprint"), Err,
                                          TEXT("BLUEPRINT_NOT_FOUND"));
  }
  return Blueprint;
}

UBlueprint *ResolveScsBlueprint(const TSharedPtr<FJsonObject> &Payload) {
  // blueprintPath wins over name for the same reason as in
  // ResolveBlueprintRequestedPath: `name` can be a component or member name.
  const FString BlueprintPath = McpGetFirstStringField(Payload, {TEXT("blueprintPath"), TEXT("name")});
  return BlueprintPath.IsEmpty() ? nullptr : LoadObject<UBlueprint>(nullptr, *BlueprintPath);
}
} // namespace McpBlueprintHandlers

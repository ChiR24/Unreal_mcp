#include "Domains/Blueprint/McpAutomationBridge_BlueprintActionContext.h"

bool UMcpAutomationBridgeSubsystem::HandleBlueprintAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT(">>> HandleBlueprintAction ENTRY: RequestId=%s RawAction='%s'"),
         *RequestId, *Action);

  McpBlueprintHandlers::FBlueprintActionContext Context =
      McpBlueprintHandlers::BuildBlueprintActionContext(
          *this, RequestId, Action, Payload, RequestingSocket);
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleBlueprintAction sanitized: CleanAction='%s' Lower='%s'"),
         *Context.CleanAction, *Context.Lower);
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleBlueprintAction invoked: RequestId=%s RawAction=%s CleanAction=%s Lower=%s"),
         *RequestId, *Action, *Context.CleanAction, *Context.Lower);

  if (!Context.bLooksBlueprint) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, VeryVerbose,
           TEXT("HandleBlueprintAction: action does not match prefix check, returning false (CleanAction='%s')"),
           *Context.CleanAction);
    return false;
  }

  McpBlueprintHandlers::DiagnosticPatternChecks(Context);

  using FBlueprintRoute = bool (*)(const McpBlueprintHandlers::FBlueprintActionContext &);
  static const FBlueprintRoute Routes[] = {
      McpBlueprintHandlers::HandleBlueprintModifyScs,
      McpBlueprintHandlers::HandleBlueprintScsWrappers,
      McpBlueprintHandlers::HandleBlueprintSetVariableMetadata,
      McpBlueprintHandlers::HandleBlueprintAddConstructionScript,
      McpBlueprintHandlers::HandleBlueprintAddVariable,
      McpBlueprintHandlers::HandleBlueprintSetDefaultLiteral,
      McpBlueprintHandlers::HandleBlueprintRemoveRenameVariable,
      McpBlueprintHandlers::HandleBlueprintAddEvent,
      McpBlueprintHandlers::HandleBlueprintRemoveEvent,
      McpBlueprintHandlers::HandleBlueprintAddFunction,
      McpBlueprintHandlers::HandleBlueprintRemoveFunction,
      McpBlueprintHandlers::HandleBlueprintSetDefaultObject,
      McpBlueprintHandlers::HandleBlueprintCompile,
      McpBlueprintHandlers::HandleBlueprintProbeCreateExists,
      McpBlueprintHandlers::HandleBlueprintGet,
      McpBlueprintHandlers::HandleBlueprintAddNode,
      McpBlueprintHandlers::HandleBlueprintConnectPins,
      McpBlueprintHandlers::HandleBlueprintEnsureProbe,
      McpBlueprintHandlers::HandleBlueprintSetMetadata,
  };

  for (FBlueprintRoute Route : Routes) {
    if (Route(Context)) {
      return true;
    }
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleBlueprintAction: checking HandleSCSAction for action='%s' (clean='%s')"),
         *Action, *Context.CleanAction);
  if (HandleSCSAction(RequestId, Context.CleanAction, Payload, RequestingSocket)) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("HandleSCSAction consumed request"));
    return true;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("HandleBlueprintAction: Action '%s' not recognized, returning false to continue dispatch."),
         *Action);
  return false;
#else
  UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
         TEXT("HandleBlueprintAction: Editor-only functionality requested in non-editor build (Action=%s)"),
         *Action);
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("Blueprint actions require editor build."),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

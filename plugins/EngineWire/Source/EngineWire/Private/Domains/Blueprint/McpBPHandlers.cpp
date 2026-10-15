#include "Domains/Blueprint/McpBPActionContext.h"

bool UEngineWireSubsystem::HandleBlueprintAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  UE_LOG(LogEngineWireSubsystem, Verbose,
         TEXT(">>> HandleBlueprintAction ENTRY: RequestId=%s RawAction='%s'"),
         *RequestId, *Action);

  McpBlueprintHandlers::FBlueprintActionContext Context =
      McpBlueprintHandlers::BuildBlueprintActionContext(
          *this, RequestId, Action, Payload, RequestingSocket);
  UE_LOG(LogEngineWireSubsystem, Verbose,
         TEXT("HandleBlueprintAction sanitized: CleanAction='%s' Lower='%s'"),
         *Context.CleanAction, *Context.Lower);
  UE_LOG(LogEngineWireSubsystem, Verbose,
         TEXT("HandleBlueprintAction invoked: RequestId=%s RawAction=%s CleanAction=%s Lower=%s"),
         *RequestId, *Action, *Context.CleanAction, *Context.Lower);

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
      McpBlueprintHandlers::HandleBlueprintImplementInterface,
      McpBlueprintHandlers::HandleBlueprintEditTimeline,
      McpBlueprintHandlers::HandleBlueprintAddBehaviour,
      McpBlueprintHandlers::HandleBlueprintAddControls,
      McpBlueprintHandlers::HandleBlueprintAddRespawn,
      McpBlueprintHandlers::HandleBlueprintAddHealthHud,
      McpBlueprintHandlers::HandleBlueprintAddCounterHud,
      McpBlueprintHandlers::HandleBlueprintAddGameOver,
      McpBlueprintHandlers::HandleBlueprintAddHint,
      McpBlueprintHandlers::HandleBlueprintCompile,
      McpBlueprintHandlers::HandleBlueprintCreateExists,
      McpBlueprintHandlers::HandleBlueprintGet,
      McpBlueprintHandlers::HandleBlueprintAddNode,
      McpBlueprintHandlers::HandleBlueprintEnsureProbe,
      McpBlueprintHandlers::HandleBlueprintSetMetadata,
      McpBlueprintHandlers::HandleBlueprintStructMakeBreakNodes,
  };

  for (FBlueprintRoute Route : Routes) {
    if (Route(Context)) {
      return true;
    }
  }

  UE_LOG(LogEngineWireSubsystem, Verbose,
         TEXT("HandleBlueprintAction: checking HandleSCSAction for action='%s' (clean='%s')"),
         *Action, *Context.CleanAction);
  if (HandleSCSAction(RequestId, Context.CleanAction, Payload, RequestingSocket)) {
    UE_LOG(LogEngineWireSubsystem, Verbose,
           TEXT("HandleSCSAction consumed request"));
    return true;
  }

  UE_LOG(LogEngineWireSubsystem, Verbose,
         TEXT("HandleBlueprintAction: Action '%s' not recognized, returning false to continue dispatch."),
         *Action);
  return false;
}

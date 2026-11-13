#include "Domains/Blueprint/McpBPActionContext.h"

bool UEngineWireSubsystem::HandleBlueprintAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  McpBlueprintHandlers::FBlueprintActionContext Context =
      McpBlueprintHandlers::BuildBlueprintActionContext(
          *this, RequestId, Action, Payload, RequestingSocket);
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
      McpBlueprintHandlers::HandleBlueprintConvertActors,
      McpBlueprintHandlers::HandleBlueprintGet,
      McpBlueprintHandlers::HandleBlueprintAddNode,
      McpBlueprintHandlers::HandleBlueprintEnsureProbe,
      McpBlueprintHandlers::HandleBlueprintSetMetadata,
      McpBlueprintHandlers::HandleBlueprintStructMakeBreakNodes,
      McpBlueprintHandlers::HandleScsAddComponent,
      McpBlueprintHandlers::HandleScsGet,
  };

  for (FBlueprintRoute Route : Routes) {
    if (Route(Context)) {
      return true;
    }
  }

  SendAutomationError(
      RequestingSocket, RequestId,
      FString::Printf(TEXT("Unknown blueprint action: %s"), *Context.CleanAction),
      TEXT("UNKNOWN_ACTION"));
  return true;
}

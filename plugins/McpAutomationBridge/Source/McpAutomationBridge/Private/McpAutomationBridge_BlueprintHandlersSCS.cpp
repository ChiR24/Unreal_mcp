#include "McpAutomationBridge_BlueprintActionContext.h"

bool UMcpAutomationBridgeSubsystem::HandleSCSAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(RequestingSocket, RequestId, false,
                           TEXT("SCS operations require valid payload"),
                           nullptr, TEXT("INVALID_PAYLOAD"));
    return true;
  }

  McpBlueprintHandlers::FBlueprintActionContext Context =
      McpBlueprintHandlers::BuildScsActionContext(
          *this, RequestId, Action, Payload, RequestingSocket);
  using FScsRoute = bool (*)(const McpBlueprintHandlers::FBlueprintActionContext &);
  static const FScsRoute Routes[] = {
      McpBlueprintHandlers::HandleScsAddComponent,
      McpBlueprintHandlers::HandleScsSetTransform,
      McpBlueprintHandlers::HandleScsRemoveComponent,
      McpBlueprintHandlers::HandleScsGet,
      McpBlueprintHandlers::HandleScsReparentComponent,
      McpBlueprintHandlers::HandleScsSetProperty,
  };

  for (FScsRoute Route : Routes) {
    if (Route(Context)) {
      return true;
    }
  }

  SendAutomationError(
      RequestingSocket, RequestId,
      FString::Printf(TEXT("Unknown blueprint action: %s"), *Context.CleanAction),
      TEXT("UNKNOWN_ACTION"));
  return true;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("SCS operations require editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

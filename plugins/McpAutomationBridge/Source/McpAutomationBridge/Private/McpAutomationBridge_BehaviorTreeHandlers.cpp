#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_BehaviorTreeHandlersPrivate.h"

#if WITH_EDITOR
#include "Modules/ModuleManager.h"

namespace {

bool BehaviorTreeEditorModuleRequired(const FString& SubAction)
{
#if MCP_HAS_BEHAVIOR_TREE_GRAPH
  return SubAction != TEXT("get_tree");
#else
  return SubAction != TEXT("create") && SubAction != TEXT("get_tree");
#endif
}

bool EnsureBehaviorTreeEditorModule(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const McpBehaviorTreeHandlers::FRequestContext& Context,
    const FString& SubAction)
{
  if (!BehaviorTreeEditorModuleRequired(SubAction) ||
      FModuleManager::Get().IsModuleLoaded(TEXT("BehaviorTreeEditor"))) {
    return true;
  }

  if (FModuleManager::Get().ModuleExists(TEXT("BehaviorTreeEditor")) &&
      FModuleManager::Get().LoadModule(TEXT("BehaviorTreeEditor"))) {
    return true;
  }

  Subsystem->SendAutomationError(
      Context.RequestingSocket,
      Context.RequestId,
      TEXT("BehaviorTreeEditor plugin is not enabled in this project. Enable the Behavior Tree Editor plugin to use Behavior Tree graph editing features."),
      TEXT("BEHAVIORTREEEDITOR_PLUGIN_NOT_ENABLED"));
  return false;
}

}
#endif

bool UMcpAutomationBridgeSubsystem::HandleBehaviorTreeAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
  if (Action != TEXT("manage_behavior_tree")) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) ||
      SubAction.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Missing 'subAction' for manage_behavior_tree"),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const McpBehaviorTreeHandlers::FRequestContext Context{
      RequestId, Payload, RequestingSocket};
  if (!EnsureBehaviorTreeEditorModule(this, Context, SubAction)) {
    return true;
  }

  if (SubAction == TEXT("create")) {
    return McpBehaviorTreeHandlers::HandleCreate(this, Context);
  }
  if (SubAction == TEXT("get_tree")) {
    return McpBehaviorTreeHandlers::HandleGetTree(this, Context);
  }

  McpBehaviorTreeHandlers::FGraphContext GraphContext{nullptr, nullptr};
  if (!McpBehaviorTreeHandlers::LoadBehaviorTreeForGraph(this, Context,
                                                         GraphContext)) {
    return true;
  }

  if (SubAction == TEXT("add_node")) {
    return McpBehaviorTreeHandlers::HandleAddNode(this, Context, GraphContext);
  }
  if (SubAction == TEXT("connect_nodes")) {
    return McpBehaviorTreeHandlers::HandleConnectNodes(this, Context,
                                                       GraphContext);
  }
  if (SubAction == TEXT("remove_node")) {
    return McpBehaviorTreeHandlers::HandleRemoveNode(this, Context,
                                                     GraphContext);
  }
  if (SubAction == TEXT("break_connections")) {
    return McpBehaviorTreeHandlers::HandleBreakConnections(this, Context,
                                                           GraphContext);
  }
  if (SubAction == TEXT("set_node_properties")) {
    return McpBehaviorTreeHandlers::HandleSetNodeProperties(this, Context,
                                                            GraphContext);
  }
  if (SubAction == TEXT("add_subnode")) {
    return McpBehaviorTreeHandlers::HandleAddSubnode(this, Context,
                                                     GraphContext);
  }

  SendAutomationError(RequestingSocket, RequestId,
                      FString::Printf(TEXT("Unknown subAction: %s"),
                                      *SubAction),
                      TEXT("INVALID_SUBACTION"));
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif
}

#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_BehaviorTreeHandlersPrivate.h"

#if WITH_EDITOR
#include "McpHandlerUtils.h"
#include "BehaviorTreeGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_BehaviorTree.h"

namespace McpBehaviorTreeHandlers {

bool LoadBehaviorTreeForGraph(UMcpAutomationBridgeSubsystem* Subsystem,
                              const FRequestContext& Context,
                              FGraphContext& OutContext)
{
  FString AssetPath;
  if (!Context.Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
      AssetPath.IsEmpty()) {
    if (!Context.Payload->TryGetStringField(TEXT("behaviorTreePath"),
                                           AssetPath) ||
        AssetPath.IsEmpty()) {
      Context.Payload->TryGetStringField(TEXT("path"), AssetPath);
    }
  }

  if (AssetPath.IsEmpty()) {
    Subsystem->SendAutomationError(
        Context.RequestingSocket, Context.RequestId,
        TEXT("Missing 'assetPath' (or 'behaviorTreePath'/'path'). Use 'create' subAction to create a new Behavior Tree first."),
        TEXT("INVALID_ARGUMENT"));
    return false;
  }

  UBehaviorTree* BehaviorTree = LoadObject<UBehaviorTree>(nullptr, *AssetPath);
  if (!BehaviorTree) {
    Subsystem->SendAutomationError(
        Context.RequestingSocket,
        Context.RequestId,
        FString::Printf(TEXT("Could not load Behavior Tree at '%s'. Use 'create' subAction to create a new Behavior Tree first."),
                        *AssetPath),
        TEXT("ASSET_NOT_FOUND"));
    return false;
  }

  UEdGraph* Graph = BehaviorTree->BTGraph;
  if (!Graph) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Behavior Tree has no graph."),
                                   TEXT("GRAPH_NOT_FOUND"));
    return false;
  }

  OutContext = FGraphContext{BehaviorTree, Graph};
  return true;
}

void UpdateBehaviorTreeAsset(const FGraphContext& Context)
{
#if MCP_HAS_BEHAVIOR_TREE_GRAPH
  if (UBehaviorTreeGraph* TypedGraph =
          Cast<UBehaviorTreeGraph>(Context.Graph)) {
    TypedGraph->UpdateAsset();
  }
#endif
  Context.Graph->NotifyGraphChanged();
  Context.BehaviorTree->MarkPackageDirty();
}

UEdGraphNode* FindGraphNodeByIdOrName(UEdGraph* Graph,
                                      const FString& IdOrName)
{
  if (!Graph || IdOrName.IsEmpty()) {
    return nullptr;
  }
  const FString Needle = IdOrName.TrimStartAndEnd();

  TFunction<UEdGraphNode*(UEdGraphNode*)> Match;
  Match = [&](UEdGraphNode* Node) -> UEdGraphNode* {
    if (!Node) return nullptr;
    if (Node->NodeGuid.ToString() == Needle) return Node;
    FGuid SearchGuid;
    if (FGuid::Parse(Needle, SearchGuid) && Node->NodeGuid == SearchGuid) {
      return Node;
    }
    if (Node->GetName().Equals(Needle, ESearchCase::IgnoreCase)) return Node;
    if (Node->GetPathName().Equals(Needle, ESearchCase::IgnoreCase)) {
      return Node;
    }
#if MCP_HAS_BEHAVIOR_TREE_GRAPH
    if (UAIGraphNode* AINode = Cast<UAIGraphNode>(Node)) {
      for (UAIGraphNode* SubNode : AINode->SubNodes) {
        if (UEdGraphNode* Found = Match(SubNode)) return Found;
      }
    }
#endif
    return nullptr;
  };

  for (UEdGraphNode* Node : Graph->Nodes) {
    if (UEdGraphNode* Found = Match(Node)) return Found;
  }
  return nullptr;
}

bool HandleConnectNodes(UMcpAutomationBridgeSubsystem* Subsystem,
                        const FRequestContext& Context,
                        const FGraphContext& GraphContext)
{
#if !MCP_HAS_BEHAVIOR_TREE_GRAPH
  Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                 TEXT("Behavior Tree graph editing requires UE 5.3+"),
                                 TEXT("NOT_SUPPORTED"));
  return true;
#else
  FString ParentNodeId, ChildNodeId;
  Context.Payload->TryGetStringField(TEXT("parentNodeId"), ParentNodeId);
  Context.Payload->TryGetStringField(TEXT("childNodeId"), ChildNodeId);

  UEdGraphNode* Parent =
      FindGraphNodeByIdOrName(GraphContext.Graph, ParentNodeId);
  UEdGraphNode* Child =
      FindGraphNodeByIdOrName(GraphContext.Graph, ChildNodeId);
  if (!Parent || !Child) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Parent or child node not found."),
                                   TEXT("NODE_NOT_FOUND"));
    return true;
  }

  UEdGraphPin* OutputPin = nullptr;
  for (UEdGraphPin* Pin : Parent->Pins) {
    if (Pin->Direction == EGPD_Output) {
      OutputPin = Pin;
      break;
    }
  }

  UEdGraphPin* InputPin = nullptr;
  for (UEdGraphPin* Pin : Child->Pins) {
    if (Pin->Direction == EGPD_Input) {
      InputPin = Pin;
      break;
    }
  }

  if (!OutputPin || !InputPin) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Could not find valid pins for connection."),
                                   TEXT("PIN_NOT_FOUND"));
    return true;
  }

  if (!GraphContext.Graph->GetSchema()->TryCreateConnection(OutputPin,
                                                            InputPin)) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Failed to connect nodes."),
                                   TEXT("CONNECT_FAILED"));
    return true;
  }

  GraphContext.BehaviorTree->Modify();
  GraphContext.Graph->Modify();
  Parent->Modify();
  Child->Modify();
  UpdateBehaviorTreeAsset(GraphContext);
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, GraphContext.BehaviorTree);
  Subsystem->SendAutomationResponse(Context.RequestingSocket,
                                    Context.RequestId, true,
                                    TEXT("Nodes connected."), Result);
  return true;
#endif
}

}
#endif

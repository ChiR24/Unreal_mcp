#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BehaviorTree/McpAutomationBridge_BehaviorTreeHandlersPrivate.h"

#if WITH_EDITOR
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "BehaviorTreeGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_BehaviorTree.h"

namespace McpBehaviorTreeHandlers {

bool EnsureBehaviorTreeGraph(UBehaviorTree*& BehaviorTree, UEdGraph*& OutGraph)
{
  if (!BehaviorTree)
  {
    return false;
  }
  OutGraph = BehaviorTree->BTGraph;
  if (OutGraph)
  {
#if MCP_HAS_BEHAVIOR_TREE_GRAPH
    // Asset-route edits (add_task_node/add_composite_node) bypass the graph: spawn their graph
    // nodes so graph-route ids resolve and UpdateAsset does not drop them (dogfood #60).
    if (UBehaviorTreeGraph* BTGraph = Cast<UBehaviorTreeGraph>(OutGraph)) { BTGraph->SpawnMissingNodes(); }
    SyncBehaviorTreeGraphFromAsset(BehaviorTree, OutGraph);
#endif
    return true;
  }
#if MCP_HAS_BEHAVIOR_TREE_GRAPH
  UEdGraph* NewGraph = NewObject<UBehaviorTreeGraph>(BehaviorTree, TEXT("BehaviorTree"));
  NewGraph->Schema = UEdGraphSchema_BehaviorTree::StaticClass();
  BehaviorTree->BTGraph = NewGraph;
  NewGraph->GetSchema()->CreateDefaultNodesForGraph(*NewGraph);
  if (UBehaviorTreeGraph* BTGraph = Cast<UBehaviorTreeGraph>(NewGraph)) { BTGraph->SpawnMissingNodes(); }
  SyncBehaviorTreeGraphFromAsset(BehaviorTree, NewGraph);
  OutGraph = NewGraph;
  return true;
#else
  return false;
#endif
}

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
  // Always run the ensure step: besides creating a missing graph it syncs asset-route nodes
  // (add_composite_node / add_task_node) into an existing graph (dogfood #60).
  if (!EnsureBehaviorTreeGraph(BehaviorTree, Graph)) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Behavior Tree graph editing requires UE 5.3+."),
                                   TEXT("NOT_SUPPORTED"));
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
  McpSafeAssetSave(Context.BehaviorTree);
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
      // Asset-route ids (BTTask_Wait_0) name the node instance, not the graph node (dogfood #60).
      if (AINode->NodeInstance && AINode->NodeInstance->GetName().Equals(Needle, ESearchCase::IgnoreCase)) return Node;
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
    FString GraphNodes;
    for (UEdGraphNode* Node : GraphContext.Graph->Nodes) {
      if (!Node) continue;
      FString Entry = Node->GetName();
      if (UAIGraphNode* AINode = Cast<UAIGraphNode>(Node)) {
        Entry += FString::Printf(TEXT(" [%s]"), AINode->NodeInstance ? *AINode->NodeInstance->GetName() : TEXT("no instance"));
      }
      GraphNodes += (GraphNodes.IsEmpty() ? TEXT("") : TEXT(", ")) + Entry;
    }
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   FString::Printf(TEXT("%s node not found in the Behavior Tree graph: %s (ids may be graph GUIDs, node instance names such as BTTask_Wait_0, or node titles). Graph nodes: %s"),
                                                   !Parent ? TEXT("Parent") : TEXT("Child"), !Parent ? *ParentNodeId : *ChildNodeId, *GraphNodes),
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

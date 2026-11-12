#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BehaviorTree/McpBTPrivate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/Reflection/McpClassResolve.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Decorator.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeGraphNode_Service.h"
#include "EdGraph/EdGraphPin.h"

namespace {

bool IsGraphNodeOfClass(UEdGraphNode* GraphNode, UClass* GraphClass)
{
  return GraphNode && GraphClass && GraphNode->GetClass()->IsChildOf(GraphClass);
}

}

namespace McpBehaviorTreeHandlers {

bool HandleAddSubnode(UEngineWireSubsystem* Subsystem,
                      const FRequestContext& Context,
                      const FGraphContext& GraphContext)
{
#if !MCP_HAS_BEHAVIOR_TREE_GRAPH
  Subsystem->SendAutomationError(
      Context.RequestingSocket, Context.RequestId,
      TEXT("add_subnode requires UE 5.3+ Behavior Tree graph editing support."),
      TEXT("NOT_SUPPORTED"));
  return true;
#else
  FString ParentNodeIdStr, SubnodeType, NodeClass;
  if (!Context.Payload->TryGetStringField(TEXT("parentNodeId"),
                                          ParentNodeIdStr) ||
      !Context.Payload->TryGetStringField(TEXT("subnodeType"), SubnodeType) ||
      !Context.Payload->TryGetStringField(TEXT("nodeClass"), NodeClass)) {
    Subsystem->SendAutomationError(
        Context.RequestingSocket, Context.RequestId,
        TEXT("add_subnode requires assetPath, parentNodeId, subnodeType, nodeClass"),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UClass* BTRootNodeClass = GraphNodeClass(TEXT("BehaviorTreeGraphNode_Root"));
  UClass* BTDecoratorNodeClass = GraphNodeClass(TEXT("BehaviorTreeGraphNode_Decorator"));
  UClass* BTServiceNodeClass = GraphNodeClass(TEXT("BehaviorTreeGraphNode_Service"));
  if (!GraphNodeClass(TEXT("BehaviorTreeGraphNode")) || !BTRootNodeClass || !BTDecoratorNodeClass ||
      !BTServiceNodeClass) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("BehaviorTreeEditor graph node classes are not loaded."),
                                   TEXT("NOT_SUPPORTED"));
    return true;
  }

  UBehaviorTreeGraphNode* ParentNode = nullptr;
  bool bRootSentinelParent = false;
  // 'root' and the Root node's own id (BehaviorTreeGraphNode_Root_0) both mean the node linked under the Root: the
  // tree build reads root decorators from that node and never from the Root node itself.
  UEdGraphNode* Resolved = FindGraphNodeByIdOrName(GraphContext.Graph, ParentNodeIdStr);
  if (ParentNodeIdStr.Equals(TEXT("root"), ESearchCase::IgnoreCase) || IsGraphNodeOfClass(Resolved, BTRootNodeClass)) {
    bRootSentinelParent = true;
    UBehaviorTreeGraphNode* RootNode = FindBehaviorTreeGraphRoot(GraphContext.Graph);
    if (!RootNode) {
      Subsystem->SendAutomationError(Context.RequestingSocket,
                                     Context.RequestId,
                                     TEXT("Root graph node not found for root sentinel parentNodeId"),
                                     TEXT("INVALID_PARENT"));
      return true;
    }
    UEdGraphPin::ResolveAllPinReferences();
    if (RootNode->Pins.Num() == 0 || !RootNode->Pins[0] ||
        RootNode->Pins[0]->LinkedTo.Num() == 0) {
      Subsystem->SendAutomationError(
          Context.RequestingSocket,
          Context.RequestId,
          TEXT("Root graph node has no linked child; connect the root to a Behavior Tree node before adding a root subnode"),
          TEXT("INVALID_PARENT"));
      return true;
    }
    UEdGraphPin* LinkedPin = RootNode->Pins[0]->LinkedTo[0];
    ParentNode = LinkedPin ? AsBehaviorTreeGraphNode(LinkedPin->GetOwningNode()) : nullptr;
    if (!ParentNode) {
      Subsystem->SendAutomationError(Context.RequestingSocket,
                                     Context.RequestId,
                                     TEXT("Root graph node's linked child is not a Behavior Tree graph node"),
                                     TEXT("INVALID_PARENT"));
      return true;
    }
  } else {
    // add_composite / add_task / add_decorator hand back ids like
    // "BTComposite_Selector_0" and take them back as parentNodeId. This guard
    // rejected exactly those before FindGraphNodeByIdOrName - which resolves
    // them - was ever consulted, and no capability returns the GUID it asked
    // for, so attaching a subnode to a named node was unreachable.
    FGuid ParentGuid;
    if (!Resolved && !FGuid::Parse(ParentNodeIdStr, ParentGuid)) {
      Subsystem->SendAutomationError(
          Context.RequestingSocket,
          Context.RequestId,
          FString::Printf(TEXT("No node named '%s'. Pass 'root', a node GUID, or a node id as returned by add_composite/add_task (e.g. BTComposite_Selector_0)."),
                          *ParentNodeIdStr),
          TEXT("INVALID_PARENT"));
      return true;
    }
    if (UEdGraphNode* Found = Resolved) {
      if (IsGraphNodeOfClass(Found, BTDecoratorNodeClass) ||
          IsGraphNodeOfClass(Found, BTServiceNodeClass)) {
        Subsystem->SendAutomationError(
            Context.RequestingSocket,
            Context.RequestId,
            FString::Printf(TEXT("Parent node %s is a Decorator/Service subnode and cannot host other subnodes"),
                            *ParentNodeIdStr),
            TEXT("INVALID_PARENT_FOR_SUBNODE"));
        return true;
      }
      ParentNode = AsBehaviorTreeGraphNode(Found);
    }
  }

  if (!ParentNode) {
    Subsystem->SendAutomationError(
        Context.RequestingSocket,
        Context.RequestId,
        FString::Printf(TEXT("Parent node not found: %s"), *ParentNodeIdStr),
        TEXT("INVALID_PARENT"));
    return true;
  }

  // nodeClass: a class name or path, or the editor's own name for a stock node (Cooldown, Blackboard, DefaultFocus),
  // which is BTDecorator_<name> or BTService_<name> in AIModule. Only a path is loaded: on a bare name LoadObject finds
  // nothing and logs a warning into the reply.
  UClass* NodeInstanceClass = McpFindTypeQuiet(NodeClass);
  if (!NodeInstanceClass) {
    NodeInstanceClass = NodeClass.Contains(TEXT("/"))
        ? LoadObject<UClass>(nullptr, *NodeClass)
        : FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/AIModule.BT%s_%s"),
              SubnodeType.Equals(TEXT("Service"), ESearchCase::IgnoreCase) ? TEXT("Service") : TEXT("Decorator"), *NodeClass));
  }
  if (!NodeInstanceClass) {
    Subsystem->SendAutomationError(
        Context.RequestingSocket,
        Context.RequestId,
        FString::Printf(TEXT("Subnode class not found: %s"), *NodeClass),
        TEXT("INVALID_CLASS"));
    return true;
  }
  // An abstract class (BTDecorator_BlackboardBase) cannot be a node: NewObject ensures and the editor nulls it on save.
  if (NodeInstanceClass->HasAnyClassFlags(CLASS_Abstract)) {
    Subsystem->SendAutomationError(
        Context.RequestingSocket,
        Context.RequestId,
        FString::Printf(TEXT("Class %s is abstract and cannot be a node; name one of its subclasses."), *NodeInstanceClass->GetName()),
        TEXT("INVALID_CLASS"));
    return true;
  }

  UClass* SubnodeGraphClass = nullptr;
  if (SubnodeType.Equals(TEXT("Decorator"), ESearchCase::IgnoreCase)) {
    if (!NodeInstanceClass->IsChildOf(UBTDecorator::StaticClass())) {
      Subsystem->SendAutomationError(
          Context.RequestingSocket,
          Context.RequestId,
          FString::Printf(TEXT("Class %s is not a UBTDecorator subclass"),
                          *NodeClass),
          TEXT("INVALID_CLASS"));
      return true;
    }
    SubnodeGraphClass = BTDecoratorNodeClass;
  } else if (SubnodeType.Equals(TEXT("Service"), ESearchCase::IgnoreCase)) {
    if (!NodeInstanceClass->IsChildOf(UBTService::StaticClass())) {
      Subsystem->SendAutomationError(
          Context.RequestingSocket,
          Context.RequestId,
          FString::Printf(TEXT("Class %s is not a UBTService subclass"),
                          *NodeClass),
          TEXT("INVALID_CLASS"));
      return true;
    }
    if (bRootSentinelParent ||
        IsGraphNodeOfClass(ParentNode, BTRootNodeClass)) {
      Subsystem->SendAutomationError(
          Context.RequestingSocket,
          Context.RequestId,
          TEXT("Service subnode cannot be attached to the root graph node — use Decorator, or attach the Service to a composite/task child."),
          TEXT("INVALID_PARENT_FOR_SUBNODE"));
      return true;
    }
    SubnodeGraphClass = BTServiceNodeClass;
  } else {
    Subsystem->SendAutomationError(
        Context.RequestingSocket,
        Context.RequestId,
        FString::Printf(TEXT("subnodeType must be 'Decorator' or 'Service' (got: %s)"),
                        *SubnodeType),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UBehaviorTreeGraphNode* NewSubnode = AsBehaviorTreeGraphNode(
      NewObject<UObject>(GraphContext.Graph, SubnodeGraphClass, NAME_None, RF_Transactional));
  if (!NewSubnode) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Failed to create Behavior Tree graph subnode."),
                                   TEXT("CREATE_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  {
    // One undo step for the subnode, its parent and the tree the graph rebuilds (AddSubNode's own step nests in it).
    const FMcpAssetEdit Edit(TEXT("Add Behavior Tree Subnode"),
                             BehaviorTreeEditObjects(GraphContext, {ParentNode, NewSubnode}), Result);
    NewSubnode->ClassData = FGraphNodeClassData(NodeInstanceClass, FString());
    NewSubnode->NodeInstance = NewObject<UBTNode>(NewSubnode, NodeInstanceClass);
    // AddSubNode gives the subnode its guid, places it and allocates its pins (a decorator or service has none).
    ParentNode->AddSubNode(NewSubnode, GraphContext.Graph);
    UpdateBehaviorTreeAsset(GraphContext);
  }
  McpSafeAssetSave(GraphContext.BehaviorTree);
  Result->SetStringField(TEXT("nodeId"),
                         NewSubnode->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens));
  Result->SetStringField(TEXT("nodeClass"), NodeInstanceClass->GetName());
  Result->SetStringField(TEXT("parentNodeId"), ParentNodeIdStr);
  Result->SetStringField(TEXT("subnodeType"), SubnodeType);
  Subsystem->SendAutomationResponse(Context.RequestingSocket,
                                    Context.RequestId, true,
                                    TEXT("Subnode added."), Result);
  return true;
#endif
}

}

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BehaviorTree/McpAutomationBridge_BehaviorTreeHandlersPrivate.h"

#if WITH_EDITOR
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTNode.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Decorator.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeGraphNode_Service.h"
#include "EdGraph/EdGraphPin.h"

namespace {

UBehaviorTreeGraphNode* AsBehaviorTreeGraphNode(UEdGraphNode* GraphNode,
                                                UClass* BTNodeBaseClass)
{
  return GraphNode && BTNodeBaseClass &&
                 GraphNode->GetClass()->IsChildOf(BTNodeBaseClass)
             ? static_cast<UBehaviorTreeGraphNode*>(GraphNode)
             : nullptr;
}

bool IsGraphNodeOfClass(UEdGraphNode* GraphNode, UClass* GraphClass)
{
  return GraphNode && GraphClass && GraphNode->GetClass()->IsChildOf(GraphClass);
}

}

namespace McpBehaviorTreeHandlers {

bool HandleAddSubnode(UMcpAutomationBridgeSubsystem* Subsystem,
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

  UClass* BTNodeBaseClass =
      FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode"));
  UClass* BTRootNodeClass =
      FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Root"));
  UClass* BTDecoratorNodeClass =
      FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Decorator"));
  UClass* BTServiceNodeClass =
      FindObject<UClass>(nullptr, TEXT("/Script/BehaviorTreeEditor.BehaviorTreeGraphNode_Service"));
  if (!BTNodeBaseClass || !BTRootNodeClass || !BTDecoratorNodeClass ||
      !BTServiceNodeClass) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("BehaviorTreeEditor graph node classes are not loaded."),
                                   TEXT("NOT_SUPPORTED"));
    return true;
  }

  UBehaviorTreeGraphNode* ParentNode = nullptr;
  bool bRootSentinelParent = false;
  if (ParentNodeIdStr.Equals(TEXT("root"), ESearchCase::IgnoreCase)) {
    bRootSentinelParent = true;
    UBehaviorTreeGraphNode* RootNode = nullptr;
    for (UEdGraphNode* GraphNode : GraphContext.Graph->Nodes) {
      if (IsGraphNodeOfClass(GraphNode, BTRootNodeClass)) {
        RootNode = AsBehaviorTreeGraphNode(GraphNode, BTNodeBaseClass);
        break;
      }
    }
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
    ParentNode = LinkedPin
                     ? AsBehaviorTreeGraphNode(LinkedPin->GetOwningNode(),
                                               BTNodeBaseClass)
                     : nullptr;
    if (!ParentNode) {
      Subsystem->SendAutomationError(Context.RequestingSocket,
                                     Context.RequestId,
                                     TEXT("Root graph node's linked child is not a Behavior Tree graph node"),
                                     TEXT("INVALID_PARENT"));
      return true;
    }
  } else {
    FGuid ParentGuid;
    if (!FGuid::Parse(ParentNodeIdStr, ParentGuid)) {
      Subsystem->SendAutomationError(
          Context.RequestingSocket,
          Context.RequestId,
          FString::Printf(TEXT("Invalid parentNodeId: %s (must be 'root' or a GUID)"),
                          *ParentNodeIdStr),
          TEXT("INVALID_PARENT"));
      return true;
    }
    if (UEdGraphNode* Found =
            FindGraphNodeByIdOrName(GraphContext.Graph, ParentNodeIdStr)) {
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
      ParentNode = AsBehaviorTreeGraphNode(Found, BTNodeBaseClass);
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

  UClass* NodeInstanceClass = UClass::TryFindTypeSlow<UClass>(NodeClass);
  if (!NodeInstanceClass) {
    NodeInstanceClass = LoadObject<UClass>(nullptr, *NodeClass);
  }
  if (!NodeInstanceClass) {
    Subsystem->SendAutomationError(
        Context.RequestingSocket,
        Context.RequestId,
        FString::Printf(TEXT("Subnode class not found: %s"), *NodeClass),
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

  UObject* NewSubnodeObj =
      NewObject<UObject>(GraphContext.Graph, SubnodeGraphClass, NAME_None,
                         RF_Transactional);
  UBehaviorTreeGraphNode* NewSubnode =
      NewSubnodeObj && NewSubnodeObj->GetClass()->IsChildOf(BTNodeBaseClass)
          ? static_cast<UBehaviorTreeGraphNode*>(NewSubnodeObj)
          : nullptr;
  if (!NewSubnode) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Failed to create Behavior Tree graph subnode."),
                                   TEXT("CREATE_FAILED"));
    return true;
  }

  NewSubnode->ClassData = FGraphNodeClassData(NodeInstanceClass, FString());
  NewSubnode->NodeInstance = NewObject<UBTNode>(NewSubnode, NodeInstanceClass);
  NewSubnode->CreateNewGuid();
  NewSubnode->PostPlacedNewNode();
  // Guard against duplicate pins: some node types already allocate in
  // PostPlacedNewNode(), so only allocate when the node has no pins yet.
  if (NewSubnode->Pins.Num() == 0) { NewSubnode->AllocateDefaultPins(); }
  ParentNode->AddSubNode(NewSubnode, GraphContext.Graph);
  UpdateBehaviorTreeAsset(GraphContext);

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
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
#endif

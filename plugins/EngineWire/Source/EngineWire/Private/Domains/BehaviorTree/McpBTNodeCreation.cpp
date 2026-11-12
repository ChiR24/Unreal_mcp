#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/BehaviorTree/McpBTPrivate.h"

#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Composites/BTComposite_SimpleParallel.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Services/BTService_DefaultFocus.h"
#include "BehaviorTree/Tasks/BTTask_FinishWithResult.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_RotateToFaceBBEntry.h"
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTreeGraphNode.h"

namespace McpBehaviorTreeHandlers {

namespace {

// add_node's nodeType: the graph-node class it is drawn as (null when the editor's class is not loaded) and the node
// it makes. One of add_node's own names first, else a Behavior Tree node class by name.
void ResolveBehaviorTreeNodeClasses(const FString& NodeType,
                                    UClass*& NodeClass,
                                    UClass*& NodeInstanceClass)
{
  static const struct { const TCHAR* NodeType; const TCHAR* GraphClass; UClass* Instance; } Named[] = {
      {TEXT("Sequence"), TEXT("BehaviorTreeGraphNode_Composite"), UBTComposite_Sequence::StaticClass()},
      {TEXT("Selector"), TEXT("BehaviorTreeGraphNode_Composite"), UBTComposite_Selector::StaticClass()},
      {TEXT("SimpleParallel"), TEXT("BehaviorTreeGraphNode_Composite"), UBTComposite_SimpleParallel::StaticClass()},
      {TEXT("Wait"), TEXT("BehaviorTreeGraphNode_Task"), UBTTask_Wait::StaticClass()},
      {TEXT("Task"), TEXT("BehaviorTreeGraphNode_Task"), UBTTask_Wait::StaticClass()},
      {TEXT("MoveTo"), TEXT("BehaviorTreeGraphNode_Task"), UBTTask_MoveTo::StaticClass()},
      {TEXT("RotateTo"), TEXT("BehaviorTreeGraphNode_Task"), UBTTask_RotateToFaceBBEntry::StaticClass()},
      {TEXT("RunBehavior"), TEXT("BehaviorTreeGraphNode_Task"), UBTTask_RunBehavior::StaticClass()},
      {TEXT("Fail"), TEXT("BehaviorTreeGraphNode_Task"), UBTTask_FinishWithResult::StaticClass()},
      {TEXT("Succeed"), TEXT("BehaviorTreeGraphNode_Task"), UBTTask_FinishWithResult::StaticClass()},
      {TEXT("Root"), TEXT("BehaviorTreeGraphNode_Root"), nullptr},
      {TEXT("Decorator"), TEXT("BehaviorTreeGraphNode_Decorator"), UBTDecorator_Blackboard::StaticClass()},
      {TEXT("Blackboard"), TEXT("BehaviorTreeGraphNode_Decorator"), UBTDecorator_Blackboard::StaticClass()},
      {TEXT("Service"), TEXT("BehaviorTreeGraphNode_Service"), UBTService_DefaultFocus::StaticClass()},
      {TEXT("DefaultFocus"), TEXT("BehaviorTreeGraphNode_Service"), UBTService_DefaultFocus::StaticClass()},
      {TEXT("Composite"), TEXT("BehaviorTreeGraphNode_Composite"), UBTComposite_Sequence::StaticClass()},
  };
  NodeClass = nullptr;
  NodeInstanceClass = nullptr;
  for (const auto& Kind : Named) {
    if (NodeType == Kind.NodeType) {
      NodeClass = GraphNodeClass(Kind.GraphClass);
      NodeInstanceClass = Kind.Instance;
      break;
    }
  }
  if (NodeClass) return;
  UClass* Resolved = ResolveClassByName(NodeType);
  if (!Resolved) return;
  const struct { const UClass* Base; const TCHAR* GraphClass; } Kinds[] = {
      {UBTCompositeNode::StaticClass(), TEXT("BehaviorTreeGraphNode_Composite")},
      {UBTTaskNode::StaticClass(), TEXT("BehaviorTreeGraphNode_Task")},
      {UBTDecorator::StaticClass(), TEXT("BehaviorTreeGraphNode_Decorator")},
      {UBTService::StaticClass(), TEXT("BehaviorTreeGraphNode_Service")},
  };
  for (const auto& Kind : Kinds) {
    if (Resolved->IsChildOf(Kind.Base)) {
      NodeClass = GraphNodeClass(Kind.GraphClass);
      NodeInstanceClass = Resolved;
      return;
    }
  }
}

}

bool HandleAddNode(UEngineWireSubsystem* Subsystem,
                   const FRequestContext& Context,
                   const FGraphContext& GraphContext)
{
#if !MCP_HAS_BEHAVIOR_TREE_GRAPH
  Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                 TEXT("Behavior Tree graph editing requires UE 5.3+"),
                                 TEXT("NOT_SUPPORTED"));
  return true;
#else
  FString NodeType;
  Context.Payload->TryGetStringField(TEXT("nodeType"), NodeType);
  float X = 0.0f;
  float Y = 0.0f;
  const bool bHasX = Context.Payload->TryGetNumberField(TEXT("x"), X);
  const bool bHasY = Context.Payload->TryGetNumberField(TEXT("y"), Y);
  if (Context.Payload->HasField(TEXT("x")) && !bHasX) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Invalid value for 'x': expected number"),
                                   TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (Context.Payload->HasField(TEXT("y")) && !bHasY) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Invalid value for 'y': expected number"),
                                   TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString ProvidedNodeId;
  Context.Payload->TryGetStringField(TEXT("nodeId"), ProvidedNodeId);
  UClass* NodeClass = nullptr;
  UClass* NodeInstanceClass = nullptr;
  ResolveBehaviorTreeNodeClasses(NodeType, NodeClass, NodeInstanceClass);

  if (!NodeClass) {
    Subsystem->SendAutomationError(
        Context.RequestingSocket, Context.RequestId,
        FString::Printf(TEXT("Unknown node type '%s'"), *NodeType),
        TEXT("UNKNOWN_TYPE"));
    return true;
  }
  // An abstract class (BTTask_BlackboardBase) cannot be a node: NewObject ensures and the editor nulls it on save.
  if (NodeInstanceClass && NodeInstanceClass->HasAnyClassFlags(CLASS_Abstract)) {
    Subsystem->SendAutomationError(
        Context.RequestingSocket, Context.RequestId,
        FString::Printf(TEXT("Class %s is abstract and cannot be a node; name one of its subclasses."), *NodeInstanceClass->GetName()),
        TEXT("INVALID_CLASS"));
    return true;
  }
  // A graph has one Root node: the editor never duplicates it, and the tree is built from the first one found.
  if (const UEdGraphNode* Root = NodeClass == GraphNodeClass(TEXT("BehaviorTreeGraphNode_Root")) ? FindBehaviorTreeGraphRoot(GraphContext.Graph) : nullptr) {
    Subsystem->SendAutomationError(
        Context.RequestingSocket, Context.RequestId,
        FString::Printf(TEXT("The graph already has its Root node (%s); connect_nodes links a node under it."), *Root->GetName()),
        TEXT("ALREADY_EXISTS"));
    return true;
  }

  UBehaviorTreeGraphNode* NewNode = AsBehaviorTreeGraphNode(
      NewObject<UObject>(GraphContext.Graph, NodeClass, NAME_None, RF_Transactional));
  if (!NewNode) {
    Subsystem->SendAutomationError(Context.RequestingSocket, Context.RequestId,
                                   TEXT("Failed to create node object."),
                                   TEXT("CREATE_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  {
    // One undo step for the node, its place in the graph and the tree the graph rebuilds.
    const FMcpAssetEdit Edit(TEXT("Add Behavior Tree Node"), BehaviorTreeEditObjects(GraphContext, {NewNode}), Result);
    NewNode->CreateNewGuid();
    if (NodeInstanceClass) {
      NewNode->ClassData = FGraphNodeClassData(NodeInstanceClass, TEXT(""));
    }
    FGuid NewGuid;
    if (!ProvidedNodeId.IsEmpty() && FGuid::Parse(ProvidedNodeId, NewGuid)) {
      NewNode->NodeGuid = NewGuid;
    }
    NewNode->NodePosX = X;
    NewNode->NodePosY = Y;
    GraphContext.Graph->AddNode(NewNode, true, false);
    // Allocate pins BEFORE PostPlacedNewNode(): checked pin accessors inside
    // PostPlacedNewNode() assert when the pin list is still empty (EdGraphNode.h:586).
    if (NewNode->Pins.Num() == 0) { NewNode->AllocateDefaultPins(); }
    NewNode->PostPlacedNewNode();

    if (NodeInstanceClass && !NewNode->NodeInstance) {
      GraphContext.Graph->RemoveNode(NewNode);
      Subsystem->SendAutomationError(
          Context.RequestingSocket, Context.RequestId,
          TEXT("Failed to initialize Behavior Tree node instance."),
          TEXT("CREATE_FAILED"));
      return true;
    }
    // "Fail" is the FinishWithResult task set to fail; its Result defaults to Succeeded. Result is an enum byte before
    // 5.5 and a value-or-Blackboard-key struct from 5.5, whose DefaultValue is the byte.
    if (NodeType == TEXT("Fail")) {
      UObject* Task = NewNode->NodeInstance;
      FProperty* ResultProperty = Task->GetClass()->FindPropertyByName(TEXT("Result"));
      void* Container = Task;
      if (const FStructProperty* Struct = CastField<FStructProperty>(ResultProperty)) {
        Container = Struct->ContainerPtrToValuePtr<void>(Task);
        ResultProperty = Struct->Struct->FindPropertyByName(TEXT("DefaultValue"));
      }
      if (const FByteProperty* Byte = CastField<FByteProperty>(ResultProperty)) {
        Byte->SetPropertyValue_InContainer(Container, static_cast<uint8>(EBTNodeResult::Failed));
      }
    }

    UpdateBehaviorTreeAsset(GraphContext);
  }
  McpSafeAssetSave(GraphContext.BehaviorTree);
  Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
  McpHandlerUtils::AddVerification(Result, GraphContext.BehaviorTree);
  Subsystem->SendAutomationResponse(Context.RequestingSocket,
                                    Context.RequestId, true,
                                    TEXT("Node added."), Result);
  return true;
#endif
}

}

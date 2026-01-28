#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridge_BlueprintHandlers_Common.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphNode_Comment.h"
#include "Engine/Blueprint.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_Event.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_InputAxisEvent.h"
#include "K2Node_Knot.h"
#include "K2Node_Literal.h"
#include "K2Node_MakeArray.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_PromotableOperator.h"
#include "K2Node_Select.h"
#include "K2Node_Self.h"
#include "K2Node_Timeline.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ScopedTransaction.h"

#endif

bool UMcpAutomationBridgeSubsystem::HandleBlueprintGraphActionInternal(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  
  const FString Lower = Action.ToLower();
  if (!Lower.StartsWith(TEXT("blueprint_add_node")) && 
      !Lower.StartsWith(TEXT("blueprint_connect_pins")) &&
      !Lower.Contains(TEXT("add_node")) &&
      !Lower.Contains(TEXT("connect_pins"))) {
    return false;
  }

  // Map legacy actions to subAction format for HandleBlueprintGraphAction
  TSharedPtr<FJsonObject> LocalPayload = MakeShared<FJsonObject>();
  for (auto& Pair : Payload->Values) {
    LocalPayload->SetField(Pair.Key, Pair.Value);
  }

  if (Lower.Contains(TEXT("add_node"))) {
    LocalPayload->SetStringField(TEXT("subAction"), TEXT("create_node"));
  } else if (Lower.Contains(TEXT("connect_pins"))) {
    LocalPayload->SetStringField(TEXT("subAction"), TEXT("connect_pins"));
  }

  return HandleBlueprintGraphAction(RequestId, TEXT("manage_blueprint_graph"), LocalPayload, RequestingSocket);
}

/**
 * Process a "manage_blueprint_graph" automation request to inspect or modify a
 * Blueprint graph.
 */
bool UMcpAutomationBridgeSubsystem::HandleBlueprintGraphAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (Action != TEXT("manage_blueprint_graph")) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Missing payload for blueprint graph action."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath = ResolveBlueprintRequestedPath(Payload);
  if (AssetPath.IsEmpty()) {
    SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Missing 'assetPath' or 'blueprintPath' in payload."),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UBlueprint *Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
  if (!Blueprint) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Could not load blueprint at path: %s"),
                        *AssetPath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  FString GraphName;
  Payload->TryGetStringField(TEXT("graphName"), GraphName);
  UEdGraph *TargetGraph = nullptr;

  // Find the target graph
  if (GraphName.IsEmpty() ||
      GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase)) {
    if (Blueprint->UbergraphPages.Num() > 0) {
      TargetGraph = Blueprint->UbergraphPages[0];
    }
  } else {
    for (UEdGraph *Graph : Blueprint->FunctionGraphs) {
      if (Graph->GetName() == GraphName) {
        TargetGraph = Graph;
        break;
      }
    }
    if (!TargetGraph) {
      for (UEdGraph *Graph : Blueprint->UbergraphPages) {
        if (Graph->GetName() == GraphName) {
          TargetGraph = Graph;
          break;
        }
      }
    }
  }

  if (!TargetGraph) {
    TArray<UEdGraph *> AllGraphs;
    Blueprint->GetAllGraphs(AllGraphs);
    for (UEdGraph *Graph : AllGraphs) {
      if (Graph->GetName() == GraphName) {
        TargetGraph = Graph;
        break;
      }
    }
  }

  if (!TargetGraph) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Could not find graph '%s' in blueprint."),
                        *GraphName),
        TEXT("GRAPH_NOT_FOUND"));
    return true;
  }

  const FString SubAction = Payload->GetStringField(TEXT("subAction"));

  auto FindNodeByIdOrName = [&](const FString &Id) -> UEdGraphNode * {
    if (Id.IsEmpty()) return nullptr;
    for (UEdGraphNode *Node : TargetGraph->Nodes) {
      if (!Node) continue;
      if (Node->NodeGuid.ToString().Equals(Id, ESearchCase::IgnoreCase) ||
          Node->GetName().Equals(Id, ESearchCase::IgnoreCase)) {
        return Node;
      }
    }
    return nullptr;
  };

  if (SubAction == TEXT("create_node")) {
    const FScopedTransaction Transaction(FText::FromString(TEXT("Create Blueprint Node")));
    Blueprint->Modify();
    TargetGraph->Modify();

    FString NodeType;
    Payload->TryGetStringField(TEXT("nodeType"), NodeType);
    float X = 0.0f;
    float Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    auto FinalizeAndReport = [&](auto &NodeCreator, UEdGraphNode *NewNode) {
      if (NewNode) {
        NewNode->NodePosX = X;
        NewNode->NodePosY = Y;
        NodeCreator.Finalize();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
        Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node created."), Result);
      } else {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create node."), TEXT("CREATE_FAILED"));
      }
    };

    static TMap<FString, TTuple<FString, FString>> CommonFunctionNodes = {
        {TEXT("PrintString"), MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("PrintString"))},
        {TEXT("Delay"), MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("Delay"))},
        {TEXT("IsValid"), MakeTuple(TEXT("UKismetSystemLibrary"), TEXT("IsValid"))}
    };

    if (const auto *FuncInfo = CommonFunctionNodes.Find(NodeType)) {
      FString ClassName = FuncInfo->Get<0>();
      FString FuncName = FuncInfo->Get<1>();
      UClass *Class = (ClassName == TEXT("UKismetSystemLibrary")) ? UKismetSystemLibrary::StaticClass() : ResolveUClass(ClassName);
      UFunction *Func = Class ? Class->FindFunctionByName(*FuncName) : nullptr;
      if (Func) {
        FGraphNodeCreator<UK2Node_CallFunction> NodeCreator(*TargetGraph);
        UK2Node_CallFunction *CallFuncNode = NodeCreator.CreateNode(false);
        CallFuncNode->SetFromFunction(Func);
        FinalizeAndReport(NodeCreator, CallFuncNode);
        return true;
      }
    }

    // Dynamic Fallback
    TArray<UClass*> NodeClasses;
    GetDerivedClasses(UEdGraphNode::StaticClass(), NodeClasses);
    for (UClass* NodeClass : NodeClasses) {
      if (NodeClass->GetName().Contains(NodeType)) {
        UEdGraphNode* NewNode = NewObject<UEdGraphNode>(TargetGraph, NodeClass);
        if (NewNode) {
          TargetGraph->AddNode(NewNode, false, false);
          NewNode->CreateNewGuid();
          NewNode->PostPlacedNewNode();
          NewNode->AllocateDefaultPins();
          NewNode->NodePosX = X;
          NewNode->NodePosY = Y;
          FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
          TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
          Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
          SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node created."), Result);
          return true;
        }
      }
    }

    SendAutomationError(RequestingSocket, RequestId, TEXT("Node type not found."), TEXT("NODE_TYPE_NOT_FOUND"));
    return true;
  } else if (SubAction == TEXT("connect_pins")) {
    const FScopedTransaction Transaction(FText::FromString(TEXT("Connect Blueprint Pins")));
    Blueprint->Modify();
    TargetGraph->Modify();

    FString FromNodeId, FromPinName, ToNodeId, ToPinName;
    Payload->TryGetStringField(TEXT("fromNodeId"), FromNodeId);
    Payload->TryGetStringField(TEXT("fromPinName"), FromPinName);
    Payload->TryGetStringField(TEXT("toNodeId"), ToNodeId);
    Payload->TryGetStringField(TEXT("toPinName"), ToPinName);

    UEdGraphNode *FromNode = FindNodeByIdOrName(FromNodeId);
    UEdGraphNode *ToNode = FindNodeByIdOrName(ToNodeId);

    if (FromNode && ToNode) {
      UEdGraphPin *FromPin = FromNode->FindPin(*FromPinName);
      UEdGraphPin *ToPin = ToNode->FindPin(*ToPinName);
      if (FromPin && ToPin) {
        if (TargetGraph->GetSchema()->TryCreateConnection(FromPin, ToPin)) {
          FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
          SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Pins connected."));
          return true;
        }
      }
    }
    SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to connect pins."), TEXT("CONNECTION_FAILED"));
    return true;
  } else if (SubAction == TEXT("delete_node") || SubAction == TEXT("bp_delete_node")) {
    // ============================================================================
    // bp_delete_node - Delete a node from the blueprint graph
    // ============================================================================
    const FScopedTransaction Transaction(FText::FromString(TEXT("Delete Blueprint Node")));
    Blueprint->Modify();
    TargetGraph->Modify();

    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);

    if (NodeId.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("nodeId is required."), TEXT("MISSING_ARGUMENT"));
      return true;
    }

    UEdGraphNode* NodeToDelete = FindNodeByIdOrName(NodeId);
    if (!NodeToDelete) {
      SendAutomationError(RequestingSocket, RequestId, 
        FString::Printf(TEXT("Node not found: %s"), *NodeId), TEXT("NODE_NOT_FOUND"));
      return true;
    }

    // Break all pin connections before deleting
    for (UEdGraphPin* Pin : NodeToDelete->Pins) {
      if (Pin) {
        Pin->BreakAllPinLinks();
      }
    }

    // Remove node from graph
    TargetGraph->RemoveNode(NodeToDelete);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("deletedNodeId"), NodeId);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node deleted successfully."), Result);
    return true;
  } else if (SubAction == TEXT("break_pin_links") || SubAction == TEXT("bp_break_pin_links")) {
    // ============================================================================
    // bp_break_pin_links - Break all connections from a specific pin
    // ============================================================================
    const FScopedTransaction Transaction(FText::FromString(TEXT("Break Pin Links")));
    Blueprint->Modify();
    TargetGraph->Modify();

    FString NodeId, PinName;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    Payload->TryGetStringField(TEXT("pinName"), PinName);

    if (NodeId.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("nodeId is required."), TEXT("MISSING_ARGUMENT"));
      return true;
    }
    if (PinName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("pinName is required."), TEXT("MISSING_ARGUMENT"));
      return true;
    }

    UEdGraphNode* TargetNode = FindNodeByIdOrName(NodeId);
    if (!TargetNode) {
      SendAutomationError(RequestingSocket, RequestId, 
        FString::Printf(TEXT("Node not found: %s"), *NodeId), TEXT("NODE_NOT_FOUND"));
      return true;
    }

    UEdGraphPin* TargetPin = TargetNode->FindPin(*PinName);
    if (!TargetPin) {
      SendAutomationError(RequestingSocket, RequestId, 
        FString::Printf(TEXT("Pin not found: %s on node %s"), *PinName, *NodeId), TEXT("PIN_NOT_FOUND"));
      return true;
    }

    int32 BrokenCount = TargetPin->LinkedTo.Num();
    TargetPin->BreakAllPinLinks();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"), NodeId);
    Result->SetStringField(TEXT("pinName"), PinName);
    Result->SetNumberField(TEXT("brokenLinkCount"), BrokenCount);
    SendAutomationResponse(RequestingSocket, RequestId, true, 
      FString::Printf(TEXT("Broke %d pin links."), BrokenCount), Result);
    return true;
  } else if (SubAction == TEXT("set_node_property") || SubAction == TEXT("bp_set_node_property")) {
    // ============================================================================
    // bp_set_node_property - Set a property on a blueprint node
    // ============================================================================
    const FScopedTransaction Transaction(FText::FromString(TEXT("Set Node Property")));
    Blueprint->Modify();
    TargetGraph->Modify();

    FString NodeId, PropertyName, PropertyValue;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
    Payload->TryGetStringField(TEXT("propertyValue"), PropertyValue);

    if (NodeId.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("nodeId is required."), TEXT("MISSING_ARGUMENT"));
      return true;
    }
    if (PropertyName.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId, TEXT("propertyName is required."), TEXT("MISSING_ARGUMENT"));
      return true;
    }

    UEdGraphNode* TargetNode = FindNodeByIdOrName(NodeId);
    if (!TargetNode) {
      SendAutomationError(RequestingSocket, RequestId, 
        FString::Printf(TEXT("Node not found: %s"), *NodeId), TEXT("NODE_NOT_FOUND"));
      return true;
    }

    // Handle common node properties
    if (PropertyName.Equals(TEXT("NodePosX"), ESearchCase::IgnoreCase)) {
      if (!PropertyValue.IsNumeric()) {
        SendAutomationError(RequestingSocket, RequestId, 
          FString::Printf(TEXT("Invalid numeric value for NodePosX: %s"), *PropertyValue), TEXT("INVALID_VALUE"));
        return true;
      }
      TargetNode->NodePosX = FCString::Atof(*PropertyValue);
    } else if (PropertyName.Equals(TEXT("NodePosY"), ESearchCase::IgnoreCase)) {
      if (!PropertyValue.IsNumeric()) {
        SendAutomationError(RequestingSocket, RequestId, 
          FString::Printf(TEXT("Invalid numeric value for NodePosY: %s"), *PropertyValue), TEXT("INVALID_VALUE"));
        return true;
      }
      TargetNode->NodePosY = FCString::Atof(*PropertyValue);
    } else if (PropertyName.Equals(TEXT("NodeComment"), ESearchCase::IgnoreCase)) {
      TargetNode->NodeComment = PropertyValue;
    } else if (PropertyName.Equals(TEXT("bCommentBubbleVisible"), ESearchCase::IgnoreCase)) {
      TargetNode->bCommentBubbleVisible = PropertyValue.ToBool();
    } else if (PropertyName.Equals(TEXT("bCommentBubblePinned"), ESearchCase::IgnoreCase)) {
      TargetNode->bCommentBubblePinned = PropertyValue.ToBool();
    } else {
      // Try to set via FProperty for custom node properties
      FProperty* Prop = TargetNode->GetClass()->FindPropertyByName(*PropertyName);
      if (Prop) {
        void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(TargetNode);
        if (Prop->ImportText_Direct(*PropertyValue, ValuePtr, TargetNode, PPF_None)) {
          // Success via FProperty
        } else {
          SendAutomationError(RequestingSocket, RequestId, 
            FString::Printf(TEXT("Failed to set property value for: %s"), *PropertyName), TEXT("PROPERTY_SET_FAILED"));
          return true;
        }
      } else {
        SendAutomationError(RequestingSocket, RequestId, 
          FString::Printf(TEXT("Property not found: %s"), *PropertyName), TEXT("PROPERTY_NOT_FOUND"));
        return true;
      }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("nodeId"), NodeId);
    Result->SetStringField(TEXT("propertyName"), PropertyName);
    Result->SetStringField(TEXT("propertyValue"), PropertyValue);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node property set successfully."), Result);
    return true;
  }

  SendAutomationError(RequestingSocket, RequestId, TEXT("Unknown subAction."), TEXT("INVALID_SUBACTION"));
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
  return true;
#endif
}

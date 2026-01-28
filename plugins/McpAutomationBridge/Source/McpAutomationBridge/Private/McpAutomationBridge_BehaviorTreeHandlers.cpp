#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "BehaviorTree/BTDecorator.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/Services/BTService_DefaultFocus.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Composites/BTComposite_SimpleParallel.h"
#include "BehaviorTree/Tasks/BTTask_FinishWithResult.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_RotateToFaceBBEntry.h"
#include "BehaviorTree/Tasks/BTTask_RunBehavior.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_Decorator.h"
#include "BehaviorTreeGraphNode_Root.h"
#include "BehaviorTreeGraphNode_Service.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_BehaviorTree.h"


#endif

/**
 * @brief Handles requests to create and manipulate Behavior Tree assets and their graphs.
 *
 * Processes the "manage_behavior_tree" action and performs editor-only subActions
 * such as "create", "add_node", "connect_nodes", "remove_node",
 * "break_connections", and "set_node_properties". Results and errors are sent
 * back over the provided websocket; when compiled without editor support an
 * appropriate error response is sent.
 *
 * @param RequestId Identifier for the incoming request; used when sending the response.
 * @param Action Action name to handle (this function acts on "manage_behavior_tree").
 * @param Payload JSON object containing a required "subAction" field and subAction-specific parameters.
 * @param RequestingSocket WebSocket to which success or error responses are written.
 * @return bool True if the request was handled (including cases where an error response was sent); false if Action does not equal "manage_behavior_tree".
 */
bool UMcpAutomationBridgeSubsystem::HandleBehaviorTreeAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
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

  // Handle 'create' subAction first - this doesn't need an existing asset
  if (SubAction == TEXT("create")) {
    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty()) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("name required for create"),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    FString SavePath;
    if (!Payload->TryGetStringField(TEXT("savePath"), SavePath) ||
        SavePath.IsEmpty()) {
      SavePath = TEXT("/Game");
    }

    // Ensure path starts with /Game
    if (!SavePath.StartsWith(TEXT("/"))) {
      SavePath = TEXT("/Game/") + SavePath;
    }

    FString PackagePath = SavePath / Name;

    // Check if already exists
    if (UEditorAssetLibrary::DoesAssetExist(PackagePath)) {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Behavior Tree already exists at %s"),
                          *PackagePath),
          TEXT("ASSET_EXISTS"));
      return true;
    }

    // Create the behavior tree asset
    UPackage *Package = CreatePackage(*PackagePath);
    if (!Package) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create package"),
                          TEXT("PACKAGE_FAILED"));
      return true;
    }

    UBehaviorTree *NewBT =
        NewObject<UBehaviorTree>(Package, UBehaviorTree::StaticClass(), FName(*Name), RF_Public | RF_Standalone);
    if (!NewBT) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to create Behavior Tree"),
                          TEXT("CREATE_FAILED"));
      return true;
    }

    // Initialize the BT graph (EdGraph)
    UEdGraph *NewGraph =
        NewObject<UBehaviorTreeGraph>(NewBT, TEXT("BehaviorTree"));
    NewGraph->Schema = UEdGraphSchema_BehaviorTree::StaticClass();
    NewBT->BTGraph = NewGraph;

    // Create default nodes (Root)
    NewGraph->GetSchema()->CreateDefaultNodesForGraph(*NewGraph);

    // Save the asset using safe helper
    FAssetRegistryModule::AssetCreated(NewBT);
    Package->MarkPackageDirty();
    bool bSaved = McpSafeAssetSave(NewBT);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("assetPath"), NewBT->GetPathName());
    Result->SetStringField(TEXT("name"), Name);
    Result->SetBoolField(TEXT("saved"), bSaved);

    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Behavior Tree created."), Result);
    return true;
  }

  FString AssetPath;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
      AssetPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Missing 'assetPath'. Use 'create' subAction to "
                             "create a new Behavior Tree first."),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  UBehaviorTree *BT = LoadObject<UBehaviorTree>(nullptr, *AssetPath);
  if (!BT) {
    SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(
            TEXT("Could not load Behavior Tree at '%s'. Use 'create' subAction "
                 "to create a new Behavior Tree first."),
            *AssetPath),
        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UEdGraph *BTGraph = BT->BTGraph;
  if (!BTGraph) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("Behavior Tree has no graph."),
                        TEXT("GRAPH_NOT_FOUND"));
    return true;
  }

  auto FindGraphNodeByIdOrName =
      [&](const FString &IdOrName) -> UEdGraphNode * {
    if (IdOrName.IsEmpty()) {
      return nullptr;
    }
    const FString Needle = IdOrName.TrimStartAndEnd();

    // Iterate nodes
    for (UEdGraphNode *Node : BTGraph->Nodes) {
      if (!Node)
        continue;

      // Check exact GUID match first
      if (Node->NodeGuid.ToString() == Needle)
        return Node;

      // Check parsed GUID match (handles format differences)
      FGuid SearchGuid;
      if (FGuid::Parse(Needle, SearchGuid) && Node->NodeGuid == SearchGuid) {
        return Node;
      }

      // Check Name and PathName
      if (Node->GetName().Equals(Needle, ESearchCase::IgnoreCase))
        return Node;
      if (Node->GetPathName().Equals(Needle, ESearchCase::IgnoreCase))
        return Node;
    }
    return nullptr;
  };

  if (SubAction == TEXT("add_node")) {
    FString NodeType;
    Payload->TryGetStringField(TEXT("nodeType"), NodeType);
    float X = 0.0f;
    float Y = 0.0f;
    Payload->TryGetNumberField(TEXT("x"), X);
    Payload->TryGetNumberField(TEXT("y"), Y);

    // Check for explicit Node ID
    FString ProvidedNodeId;
    Payload->TryGetStringField(TEXT("nodeId"), ProvidedNodeId);

    UBehaviorTreeGraphNode *NewNode = nullptr;

    // Determine node class
    UClass *NodeClass = nullptr;
    UClass *NodeInstanceClass = nullptr;

    if (NodeType == TEXT("Sequence")) {
      NodeClass = UBehaviorTreeGraphNode_Composite::StaticClass();
      NodeInstanceClass = UBTComposite_Sequence::StaticClass();
    } else if (NodeType == TEXT("Selector")) {
      NodeClass = UBehaviorTreeGraphNode_Composite::StaticClass();
      NodeInstanceClass = UBTComposite_Selector::StaticClass();
    } else if (NodeType == TEXT("SimpleParallel")) {
      NodeClass = UBehaviorTreeGraphNode_Composite::StaticClass();
      NodeInstanceClass = UBTComposite_SimpleParallel::StaticClass();
    } else if (NodeType == TEXT("Wait")) {
      NodeClass = UBehaviorTreeGraphNode_Task::StaticClass();
      NodeInstanceClass = UBTTask_Wait::StaticClass();
    } else if (NodeType == TEXT("MoveTo")) {
      NodeClass = UBehaviorTreeGraphNode_Task::StaticClass();
      NodeInstanceClass = UBTTask_MoveTo::StaticClass();
    } else if (NodeType == TEXT("RotateTo")) {
      NodeClass = UBehaviorTreeGraphNode_Task::StaticClass();
      NodeInstanceClass = UBTTask_RotateToFaceBBEntry::StaticClass();
    } else if (NodeType == TEXT("RunBehavior")) {
      NodeClass = UBehaviorTreeGraphNode_Task::StaticClass();
      NodeInstanceClass = UBTTask_RunBehavior::StaticClass();
    } else if (NodeType == TEXT("Fail")) {
      NodeClass = UBehaviorTreeGraphNode_Task::StaticClass();
      NodeInstanceClass = UBTTask_FinishWithResult::StaticClass();
    } else if (NodeType == TEXT("Succeed")) {
      // Succeed is a FinishWithResult task configured to success
      NodeClass = UBehaviorTreeGraphNode_Task::StaticClass();
      NodeInstanceClass = UBTTask_FinishWithResult::StaticClass();
    } else if (NodeType == TEXT("Root")) {
      NodeClass = UBehaviorTreeGraphNode_Root::StaticClass();
      // Root doesn't have an instance class in the same way
    } else if (NodeType == TEXT("Task")) {
      // Generic Task - creates a Wait task as default
      NodeClass = UBehaviorTreeGraphNode_Task::StaticClass();
      NodeInstanceClass = UBTTask_Wait::StaticClass();
    } else if (NodeType == TEXT("Decorator") || NodeType == TEXT("Blackboard")) {
      // Generic Decorator - creates a Blackboard decorator as default
      NodeClass = UBehaviorTreeGraphNode_Decorator::StaticClass();
      NodeInstanceClass = UBTDecorator_Blackboard::StaticClass();
    } else if (NodeType == TEXT("Service") || NodeType == TEXT("DefaultFocus")) {
      // Generic Service - creates a DefaultFocus service as default
      NodeClass = UBehaviorTreeGraphNode_Service::StaticClass();
      NodeInstanceClass = UBTService_DefaultFocus::StaticClass();
    } else if (NodeType == TEXT("Composite")) {
      // Generic Composite - creates a Sequence as default
      NodeClass = UBehaviorTreeGraphNode_Composite::StaticClass();
      NodeInstanceClass = UBTComposite_Sequence::StaticClass();
    } else {
      // Try to resolve as a class path
      UClass *Resolved = ResolveClassByName(NodeType);
      if (Resolved) {
        if (Resolved->IsChildOf(UBTCompositeNode::StaticClass())) {
          NodeClass = UBehaviorTreeGraphNode_Composite::StaticClass();
          NodeInstanceClass = Resolved;
        } else if (Resolved->IsChildOf(UBTTaskNode::StaticClass())) {
          NodeClass = UBehaviorTreeGraphNode_Task::StaticClass();
          NodeInstanceClass = Resolved;
        } else if (Resolved->IsChildOf(UBTDecorator::StaticClass())) {
          NodeClass = UBehaviorTreeGraphNode_Decorator::StaticClass();
          NodeInstanceClass = Resolved;
        } else if (Resolved->IsChildOf(UBTService::StaticClass())) {
          NodeClass = UBehaviorTreeGraphNode_Service::StaticClass();
          NodeInstanceClass = Resolved;
        }
      }
    }

    if (NodeClass) {
      NewNode = NewObject<UBehaviorTreeGraphNode>(BTGraph, NodeClass);
      if (NewNode) {

        // Use provided ID if valid, otherwise create new random one
        FGuid NewGuid;
        if (!ProvidedNodeId.IsEmpty() &&
            FGuid::Parse(ProvidedNodeId, NewGuid)) {
          NewNode->NodeGuid = NewGuid;
        } else {
          NewNode->CreateNewGuid();
        }

        NewNode->NodePosX = X;
        NewNode->NodePosY = Y;

        BTGraph->AddNode(NewNode, true, false);

        // Initialize the node instance
        NewNode->PostPlacedNewNode();
        NewNode->AllocateDefaultPins();

        BTGraph->NotifyGraphChanged();
        BT->MarkPackageDirty();

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Node added."), Result);
      } else {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Failed to create node object."),
                            TEXT("CREATE_FAILED"));
      }
    } else {
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Unknown node type '%s'"), *NodeType),
          TEXT("UNKNOWN_TYPE"));
    }
    return true;
  } else if (SubAction == TEXT("connect_nodes")) {
    // Parent -> Child connection
    FString ParentNodeId, ChildNodeId;
    Payload->TryGetStringField(TEXT("parentNodeId"), ParentNodeId);
    Payload->TryGetStringField(TEXT("childNodeId"), ChildNodeId);

    UEdGraphNode *Parent = FindGraphNodeByIdOrName(ParentNodeId);
    UEdGraphNode *Child = FindGraphNodeByIdOrName(ChildNodeId);

    if (!Parent || !Child) {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Parent or child node not found."),
                          TEXT("NODE_NOT_FOUND"));
      return true;
    }

    // In BT, output pin of parent connects to input pin of child
    UEdGraphPin *OutputPin = nullptr;
    for (UEdGraphPin *Pin : Parent->Pins) {
      if (Pin->Direction == EGPD_Output) {
        OutputPin = Pin;
        break;
      }
    }

    UEdGraphPin *InputPin = nullptr;
    for (UEdGraphPin *Pin : Child->Pins) {
      if (Pin->Direction == EGPD_Input) {
        InputPin = Pin;
        break;
      }
    }

    if (OutputPin && InputPin) {
      if (BTGraph->GetSchema()->TryCreateConnection(OutputPin, InputPin)) {
        BTGraph->NotifyGraphChanged();
        BT->MarkPackageDirty();
        SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Nodes connected."));
      } else {
        SendAutomationError(RequestingSocket, RequestId,
                            TEXT("Failed to connect nodes."),
                            TEXT("CONNECT_FAILED"));
      }
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Could not find valid pins for connection."),
                          TEXT("PIN_NOT_FOUND"));
    }
    return true;
  } else if (SubAction == TEXT("remove_node")) {
    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);

    UEdGraphNode *TargetNode = FindGraphNodeByIdOrName(NodeId);

    if (TargetNode) {
      BTGraph->RemoveNode(TargetNode);
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Node removed."));
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
    }
    return true;
  } else if (SubAction == TEXT("break_connections")) {
    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);

    UEdGraphNode *TargetNode = FindGraphNodeByIdOrName(NodeId);

    if (TargetNode) {
      TargetNode->BreakAllNodeLinks();
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Connections broken."));
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
    }
    return true;
  } else if (SubAction == TEXT("set_node_properties")) {
    FString NodeId;
    Payload->TryGetStringField(TEXT("nodeId"), NodeId);

    UEdGraphNode *TargetNode = FindGraphNodeByIdOrName(NodeId);

    if (TargetNode) {
      bool bModified = false;
      FString Comment;
      if (Payload->TryGetStringField(TEXT("comment"), Comment)) {
        TargetNode->NodeComment = Comment;
        bModified = true;
      }

      // Try to set properties on the underlying NodeInstance
      UBehaviorTreeGraphNode *BTNode = Cast<UBehaviorTreeGraphNode>(TargetNode);
      const TSharedPtr<FJsonObject> *Props = nullptr;
      if (BTNode && BTNode->NodeInstance &&
          Payload->TryGetObjectField(TEXT("properties"), Props)) {
        for (const auto &Pair : (*Props)->Values) {
          FProperty *Prop =
              BTNode->NodeInstance->GetClass()->FindPropertyByName(*Pair.Key);
          if (Prop) {
            if (FFloatProperty *FloatProp = CastField<FFloatProperty>(Prop)) {
              if (Pair.Value->Type == EJson::Number) {
                FloatProp->SetPropertyValue_InContainer(
                    BTNode->NodeInstance, (float)Pair.Value->AsNumber());
                bModified = true;
              }
            } else if (FDoubleProperty *DoubleProp =
                           CastField<FDoubleProperty>(Prop)) {
              if (Pair.Value->Type == EJson::Number) {
                DoubleProp->SetPropertyValue_InContainer(
                    BTNode->NodeInstance, Pair.Value->AsNumber());
                bModified = true;
              }
            } else if (FIntProperty *IntProp = CastField<FIntProperty>(Prop)) {
              if (Pair.Value->Type == EJson::Number) {
                IntProp->SetPropertyValue_InContainer(
                    BTNode->NodeInstance, (int32)Pair.Value->AsNumber());
                bModified = true;
              }
            } else if (FBoolProperty *BoolProp =
                           CastField<FBoolProperty>(Prop)) {
              if (Pair.Value->Type == EJson::Boolean) {
                BoolProp->SetPropertyValue_InContainer(BTNode->NodeInstance,
                                                       Pair.Value->AsBool());
                bModified = true;
              }
            } else if (FStrProperty *StrProp = CastField<FStrProperty>(Prop)) {
              if (Pair.Value->Type == EJson::String) {
                StrProp->SetPropertyValue_InContainer(BTNode->NodeInstance,
                                                      Pair.Value->AsString());
                bModified = true;
              }
            } else if (FNameProperty *NameProp =
                           CastField<FNameProperty>(Prop)) {
              if (Pair.Value->Type == EJson::String) {
                NameProp->SetPropertyValue_InContainer(
                    BTNode->NodeInstance, FName(*Pair.Value->AsString()));
                bModified = true;
              }
            }
          }
        }
      }

      if (bModified) {
        BTGraph->NotifyGraphChanged();
        BT->MarkPackageDirty();
      }

      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Node properties updated."));
    } else {
      SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."),
                          TEXT("NODE_NOT_FOUND"));
    }
    return true;
  }

  SendAutomationError(
      RequestingSocket, RequestId,
      FString::Printf(TEXT("Unknown subAction: %s"), *SubAction),
      TEXT("INVALID_SUBACTION"));
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."),
                      TEXT("EDITOR_ONLY"));
  return true;
#endif
}
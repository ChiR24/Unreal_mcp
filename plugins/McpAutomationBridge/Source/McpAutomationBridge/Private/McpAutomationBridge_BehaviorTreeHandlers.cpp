#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "BehaviorTreeGraphNode_Composite.h"
#include "BehaviorTreeGraphNode_Task.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleBehaviorTreeAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_behavior_tree"))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBehaviorTree* BT = LoadObject<UBehaviorTree>(nullptr, *AssetPath);
    if (!BT)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Could not load Behavior Tree."), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    UEdGraph* BTGraph = BT->BTGraph;
    if (!BTGraph)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Behavior Tree has no graph."), TEXT("GRAPH_NOT_FOUND"));
        return true;
    }

    FString SubAction = Payload->GetStringField(TEXT("subAction"));

    if (SubAction == TEXT("add_node"))
    {
        FString NodeType;
        Payload->TryGetStringField(TEXT("nodeType"), NodeType);
        float X = 0.0f;
        float Y = 0.0f;
        Payload->TryGetNumberField(TEXT("x"), X);
        Payload->TryGetNumberField(TEXT("y"), Y);

        UBehaviorTreeGraphNode* NewNode = nullptr;

        // Determine node class
        UClass* NodeClass = nullptr;
        UClass* NodeInstanceClass = nullptr;

        if (NodeType == TEXT("Sequence"))
        {
            NodeClass = UBehaviorTreeGraphNode_Composite::StaticClass();
            NodeInstanceClass = UBTComposite_Sequence::StaticClass();
        }
        else if (NodeType == TEXT("Selector"))
        {
            NodeClass = UBehaviorTreeGraphNode_Composite::StaticClass();
            NodeInstanceClass = UBTComposite_Selector::StaticClass();
        }
        else if (NodeType == TEXT("Wait"))
        {
            NodeClass = UBehaviorTreeGraphNode_Task::StaticClass();
            NodeInstanceClass = UBTTask_Wait::StaticClass();
        }
        // Add more types...

        if (NodeClass)
        {
            NewNode = NewObject<UBehaviorTreeGraphNode>(BTGraph, NodeClass);
            if (NewNode)
            {
                NewNode->CreateNewGuid();
                NewNode->NodePosX = X;
                NewNode->NodePosY = Y;
                
                if (NodeInstanceClass)
                {
                    if (UBehaviorTreeGraphNode_Composite* CompNode = Cast<UBehaviorTreeGraphNode_Composite>(NewNode))
                    {
                        // CompNode->ClassData.NodeClass = NodeInstanceClass; // NodeClass not accessible directly
                    }
                    else if (UBehaviorTreeGraphNode_Task* TaskNode = Cast<UBehaviorTreeGraphNode_Task>(NewNode))
                    {
                        // TaskNode->ClassData.NodeClass = NodeInstanceClass; // NodeClass not accessible directly
                    }
                }

                BTGraph->AddNode(NewNode, true, false);
                
                // Initialize the node instance
                NewNode->PostPlacedNewNode();
                NewNode->AllocateDefaultPins();

                BTGraph->NotifyGraphChanged();
                BT->MarkPackageDirty();

                TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
                Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node added."), Result);
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create node object."), TEXT("CREATE_FAILED"));
            }
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown node type '%s'"), *NodeType), TEXT("UNKNOWN_TYPE"));
        }
        return true;
    }
    else if (SubAction == TEXT("connect_nodes"))
    {
        // Parent -> Child connection
        FString ParentNodeId, ChildNodeId;
        Payload->TryGetStringField(TEXT("parentNodeId"), ParentNodeId);
        Payload->TryGetStringField(TEXT("childNodeId"), ChildNodeId);

        UEdGraphNode* Parent = nullptr;
        UEdGraphNode* Child = nullptr;

        for (UEdGraphNode* Node : BTGraph->Nodes)
        {
            if (Node->NodeGuid.ToString() == ParentNodeId) Parent = Node;
            if (Node->NodeGuid.ToString() == ChildNodeId) Child = Node;
        }

        if (!Parent || !Child)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Parent or child node not found."), TEXT("NODE_NOT_FOUND"));
            return true;
        }

        // In BT, output pin of parent connects to input pin of child
        UEdGraphPin* OutputPin = nullptr;
        for (UEdGraphPin* Pin : Parent->Pins)
        {
            if (Pin->Direction == EGPD_Output) { OutputPin = Pin; break; }
        }

        UEdGraphPin* InputPin = nullptr;
        for (UEdGraphPin* Pin : Child->Pins)
        {
            if (Pin->Direction == EGPD_Input) { InputPin = Pin; break; }
        }

        if (OutputPin && InputPin)
        {
            if (BTGraph->GetSchema()->TryCreateConnection(OutputPin, InputPin))
            {
                BTGraph->NotifyGraphChanged();
                BT->MarkPackageDirty();
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Nodes connected."));
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to connect nodes."), TEXT("CONNECT_FAILED"));
            }
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not find valid pins for connection."), TEXT("PIN_NOT_FOUND"));
        }
        return true;
    }
    else if (SubAction == TEXT("remove_node"))
    {
        FString NodeId;
        Payload->TryGetStringField(TEXT("nodeId"), NodeId);

        UEdGraphNode* TargetNode = nullptr;
        for (UEdGraphNode* Node : BTGraph->Nodes)
        {
            if (Node->NodeGuid.ToString() == NodeId)
            {
                TargetNode = Node;
                break;
            }
        }

        if (TargetNode)
        {
            BTGraph->RemoveNode(TargetNode);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node removed."));
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        }
        return true;
    }
    else if (SubAction == TEXT("break_connections"))
    {
        FString NodeId;
        Payload->TryGetStringField(TEXT("nodeId"), NodeId);

        UEdGraphNode* TargetNode = nullptr;
        for (UEdGraphNode* Node : BTGraph->Nodes)
        {
            if (Node->NodeGuid.ToString() == NodeId)
            {
                TargetNode = Node;
                break;
            }
        }

        if (TargetNode)
        {
            TargetNode->BreakAllNodeLinks();
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Connections broken."));
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        }
        return true;
    }
    else if (SubAction == TEXT("set_node_properties"))
    {
        FString NodeId;
        Payload->TryGetStringField(TEXT("nodeId"), NodeId);
        // Simplified: Only supporting node name/comment for now
        FString Comment;
        Payload->TryGetStringField(TEXT("comment"), Comment);

        UEdGraphNode* TargetNode = nullptr;
        for (UEdGraphNode* Node : BTGraph->Nodes)
        {
            if (Node->NodeGuid.ToString() == NodeId)
            {
                TargetNode = Node;
                break;
            }
        }

        if (TargetNode)
        {
            TargetNode->NodeComment = Comment;
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node properties updated."));
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        }
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown subAction: %s"), *SubAction), TEXT("INVALID_SUBACTION"));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}


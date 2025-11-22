#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Misc/ScopeExit.h"

#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Event.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "K2Node_PromotableOperator.h"
#include "K2Node_Literal.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_Knot.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleBlueprintGraphAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    if (Action != TEXT("manage_blueprint_graph"))
    {
        return false;
    }

#if WITH_EDITOR
    if (!Payload.IsValid())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing payload for blueprint graph action."), TEXT("INVALID_PAYLOAD"));
        return true;
    }

    FString AssetPath;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Missing 'assetPath' in payload."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
    if (!Blueprint)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Could not load blueprint at path: %s"), *AssetPath), TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    FString GraphName;
    Payload->TryGetStringField(TEXT("graphName"), GraphName);
    UEdGraph* TargetGraph = nullptr;

    // Find the target graph
    if (GraphName.IsEmpty() || GraphName.Equals(TEXT("EventGraph"), ESearchCase::IgnoreCase))
    {
        // Default to the main ubergraph/event graph
        if (Blueprint->UbergraphPages.Num() > 0)
        {
            TargetGraph = Blueprint->UbergraphPages[0];
        }
    }
    else
    {
        // Search in FunctionGraphs and UbergraphPages
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph->GetName() == GraphName)
            {
                TargetGraph = Graph;
                break;
            }
        }
        if (!TargetGraph)
        {
            for (UEdGraph* Graph : Blueprint->UbergraphPages)
            {
                if (Graph->GetName() == GraphName)
                {
                    TargetGraph = Graph;
                    break;
                }
            }
        }
    }

    if (!TargetGraph)
    {
        // Fallback: try finding by name in all graphs
        TArray<UEdGraph*> AllGraphs;
        Blueprint->GetAllGraphs(AllGraphs);
        for (UEdGraph* Graph : AllGraphs)
        {
            if (Graph->GetName() == GraphName)
            {
                TargetGraph = Graph;
                break;
            }
        }
    }

    if (!TargetGraph)
    {
        SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Could not find graph '%s' in blueprint."), *GraphName), TEXT("GRAPH_NOT_FOUND"));
        return true;
    }

    const FString SubAction = Payload->GetStringField(TEXT("subAction"));

    if (SubAction == TEXT("create_node"))
    {
        FString NodeType;
        Payload->TryGetStringField(TEXT("nodeType"), NodeType);
        float X = 0.0f;
        float Y = 0.0f;
        Payload->TryGetNumberField(TEXT("x"), X);
        Payload->TryGetNumberField(TEXT("y"), Y);

        FGraphNodeCreator<UEdGraphNode> NodeCreator(*TargetGraph);
        UEdGraphNode* NewNode = nullptr;

        // Basic node creation logic - this can be expanded significantly
        if (NodeType == TEXT("CallFunction"))
        {
            FString MemberName;
            Payload->TryGetStringField(TEXT("memberName"), MemberName);
            FString MemberClass;
            Payload->TryGetStringField(TEXT("memberClass"), MemberClass); // Optional, for static functions

            UK2Node_CallFunction* CallFuncNode = NewObject<UK2Node_CallFunction>(TargetGraph);
            
            UFunction* Func = nullptr;
            if (!MemberClass.IsEmpty())
            {
                UClass* Class = FindObject<UClass>(nullptr, *MemberClass);
                if (Class)
                {
                    Func = Class->FindFunctionByName(*MemberName);
                }
            }
            else
            {
                // Try to find in blueprint context
                 Func = Blueprint->GeneratedClass->FindFunctionByName(*MemberName);
                 if (!Func)
                 {
                     // Try global search if simple name
                     Func = FindObject<UFunction>(nullptr, *MemberName);
                 }
            }

            if (Func)
            {
                CallFuncNode->SetFromFunction(Func);
                NewNode = CallFuncNode;
            }
            else
            {
                 SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Could not find function '%s'"), *MemberName), TEXT("FUNCTION_NOT_FOUND"));
                 return true;
            }
        }
        else if (NodeType == TEXT("VariableGet"))
        {
             FString VarName;
             Payload->TryGetStringField(TEXT("variableName"), VarName);
             UK2Node_VariableGet* VarGet = NewObject<UK2Node_VariableGet>(TargetGraph);
             FName VarFName(*VarName);
             // Basic check if variable exists
             bool bFound = false;
             for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
             {
                 if (VarDesc.VarName == VarFName)
                 {
                     bFound = true;
                     break;
                 }
             }
             if (bFound)
             {
                 VarGet->VariableReference.SetSelfMember(VarFName);
                 NewNode = VarGet;
             }
             else
             {
                 // Try to find property in parent class
                  if (Blueprint->ParentClass && Blueprint->ParentClass->FindPropertyByName(VarFName))
                  {
                      VarGet->VariableReference.SetSelfMember(VarFName);
                      NewNode = VarGet;
                  }
                  else
                  {
                      SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Could not find variable '%s'"), *VarName), TEXT("VARIABLE_NOT_FOUND"));
                      return true;
                  }
             }
        }
        else if (NodeType == TEXT("VariableSet"))
        {
             FString VarName;
             Payload->TryGetStringField(TEXT("variableName"), VarName);
             UK2Node_VariableSet* VarSet = NewObject<UK2Node_VariableSet>(TargetGraph);
             FName VarFName(*VarName);
             bool bFound = false;
             for (const FBPVariableDescription& VarDesc : Blueprint->NewVariables)
             {
                 if (VarDesc.VarName == VarFName)
                 {
                     bFound = true;
                     break;
                 }
             }
             if (bFound || (Blueprint->ParentClass && Blueprint->ParentClass->FindPropertyByName(VarFName)))
             {
                 VarSet->VariableReference.SetSelfMember(VarFName);
                 NewNode = VarSet;
             }
             else
             {
                 SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Could not find variable '%s'"), *VarName), TEXT("VARIABLE_NOT_FOUND"));
                 return true;
             }
        }
        else if (NodeType == TEXT("CustomEvent"))
        {
            FString EventName;
            Payload->TryGetStringField(TEXT("eventName"), EventName);
            UK2Node_CustomEvent* EventNode = NewObject<UK2Node_CustomEvent>(TargetGraph);
            EventNode->CustomFunctionName = FName(*EventName);
            NewNode = EventNode;
        }
        else if (NodeType == TEXT("Branch"))
        {
            NewNode = NewObject<UK2Node_IfThenElse>(TargetGraph);
        }
        else if (NodeType == TEXT("Sequence"))
        {
            NewNode = NewObject<UK2Node_ExecutionSequence>(TargetGraph);
        }
        else if (NodeType == TEXT("Literal"))
        {
            // Simplified literal creation
            // Needs type info
            SendAutomationError(RequestingSocket, RequestId, TEXT("Literal node creation requires type specifics (not fully implemented)."), TEXT("NOT_IMPLEMENTED"));
            return true;
        }

        if (NewNode)
        {
            TargetGraph->AddNode(NewNode, true, false);
            NewNode->NodePosX = X;
            NewNode->NodePosY = Y;
            NewNode->AllocateDefaultPins();
            NewNode->GetGraph()->NotifyGraphChanged();
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
            Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node created."), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create node (unsupported type or internal error)."), TEXT("CREATE_FAILED"));
        }
        return true;
    }
    else if (SubAction == TEXT("connect_pins"))
    {
        FString FromNodeId, FromPinName, ToNodeId, ToPinName;
        Payload->TryGetStringField(TEXT("fromNodeId"), FromNodeId);
        Payload->TryGetStringField(TEXT("fromPinName"), FromPinName);
        Payload->TryGetStringField(TEXT("toNodeId"), ToNodeId);
        Payload->TryGetStringField(TEXT("toPinName"), ToPinName);

        UEdGraphNode* FromNode = nullptr;
        UEdGraphNode* ToNode = nullptr;

        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
            if (Node->NodeGuid.ToString() == FromNodeId) FromNode = Node;
            if (Node->NodeGuid.ToString() == ToNodeId) ToNode = Node;
        }

        if (!FromNode || !ToNode)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Could not find source or target node."), TEXT("NODE_NOT_FOUND"));
            return true;
        }

        UEdGraphPin* FromPin = FromNode->FindPin(*FromPinName);
        UEdGraphPin* ToPin = ToNode->FindPin(*ToPinName);

        if (!FromPin || !ToPin)
        {
             SendAutomationError(RequestingSocket, RequestId, TEXT("Could not find source or target pin."), TEXT("PIN_NOT_FOUND"));
             return true;
        }

        if (TargetGraph->GetSchema()->TryCreateConnection(FromPin, ToPin))
        {
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Pins connected."));
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to connect pins (schema rejection)."), TEXT("CONNECTION_FAILED"));
        }
        return true;
    }
    else if (SubAction == TEXT("break_pin_links"))
    {
        FString NodeId, PinName;
        Payload->TryGetStringField(TEXT("nodeId"), NodeId);
        Payload->TryGetStringField(TEXT("pinName"), PinName);

        UEdGraphNode* TargetNode = nullptr;
        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
            if (Node->NodeGuid.ToString() == NodeId)
            {
                TargetNode = Node;
                break;
            }
        }

        if (!TargetNode)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
            return true;
        }

        UEdGraphPin* Pin = TargetNode->FindPin(*PinName);
        if (!Pin)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Pin not found."), TEXT("PIN_NOT_FOUND"));
            return true;
        }

        TargetGraph->GetSchema()->BreakPinLinks(*Pin, true);
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Pin links broken."));
        return true;
    }

    else if (SubAction == TEXT("delete_node"))
    {
        FString NodeId;
        Payload->TryGetStringField(TEXT("nodeId"), NodeId);

        UEdGraphNode* TargetNode = nullptr;
        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
            if (Node->NodeGuid.ToString() == NodeId)
            {
                TargetNode = Node;
                break;
            }
        }

        if (TargetNode)
        {
            FBlueprintEditorUtils::RemoveNode(Blueprint, TargetNode, true);
            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node deleted."));
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        }
        return true;
    }
    else if (SubAction == TEXT("create_reroute_node"))
    {
        float X = 0.0f;
        float Y = 0.0f;
        Payload->TryGetNumberField(TEXT("x"), X);
        Payload->TryGetNumberField(TEXT("y"), Y);

        UK2Node_Knot* RerouteNode = NewObject<UK2Node_Knot>(TargetGraph);
        TargetGraph->AddNode(RerouteNode, true, false);
        RerouteNode->NodePosX = X;
        RerouteNode->NodePosY = Y;
        RerouteNode->AllocateDefaultPins();
        RerouteNode->GetGraph()->NotifyGraphChanged();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("nodeId"), RerouteNode->NodeGuid.ToString());
        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Reroute node created."), Result);
        return true;
    }
    else if (SubAction == TEXT("set_node_property"))
    {
        // Simplified property setting - currently only supports setting 'Comment'
        FString NodeId;
        Payload->TryGetStringField(TEXT("nodeId"), NodeId);
        FString PropertyName;
        Payload->TryGetStringField(TEXT("propertyName"), PropertyName);
        FString Value;
        Payload->TryGetStringField(TEXT("value"), Value);

        UEdGraphNode* TargetNode = nullptr;
        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
            if (Node->NodeGuid.ToString() == NodeId)
            {
                TargetNode = Node;
                break;
            }
        }

        if (TargetNode)
        {
            if (PropertyName.Equals(TEXT("Comment"), ESearchCase::IgnoreCase))
            {
                TargetNode->NodeComment = Value;
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node comment updated."));
            }
            else
            {
                SendAutomationError(RequestingSocket, RequestId, TEXT("Only 'Comment' property is currently supported."), TEXT("NOT_IMPLEMENTED"));
            }
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        }
        return true;
    }
    else if (SubAction == TEXT("get_node_details"))
    {
        FString NodeId;
        Payload->TryGetStringField(TEXT("nodeId"), NodeId);

        UEdGraphNode* TargetNode = nullptr;
        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
            if (Node->NodeGuid.ToString() == NodeId)
            {
                TargetNode = Node;
                break;
            }
        }

        if (TargetNode)
        {
            TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("nodeName"), TargetNode->GetName());
            Result->SetStringField(TEXT("nodeTitle"), TargetNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
            Result->SetStringField(TEXT("nodeComment"), TargetNode->NodeComment);
            Result->SetNumberField(TEXT("x"), TargetNode->NodePosX);
            Result->SetNumberField(TEXT("y"), TargetNode->NodePosY);
            
            TArray<TSharedPtr<FJsonValue>> Pins;
            for (UEdGraphPin* Pin : TargetNode->Pins)
            {
                TSharedPtr<FJsonObject> PinObj = MakeShared<FJsonObject>();
                PinObj->SetStringField(TEXT("pinName"), Pin->PinName.ToString());
                PinObj->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
                PinObj->SetStringField(TEXT("pinType"), Pin->PinType.PinCategory.ToString());
                Pins.Add(MakeShared<FJsonValueObject>(PinObj));
            }
            Result->SetArrayField(TEXT("pins"), Pins);

            SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Node details retrieved."), Result);
        }
        else
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        }
        return true;
    }
    else if (SubAction == TEXT("get_graph_details"))
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("graphName"), TargetGraph->GetName());
        Result->SetNumberField(TEXT("nodeCount"), TargetGraph->Nodes.Num());
        
        TArray<TSharedPtr<FJsonValue>> Nodes;
        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
            TSharedPtr<FJsonObject> NodeObj = MakeShared<FJsonObject>();
            NodeObj->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
            NodeObj->SetStringField(TEXT("nodeName"), Node->GetName());
            NodeObj->SetStringField(TEXT("nodeTitle"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
            Nodes.Add(MakeShared<FJsonValueObject>(NodeObj));
        }
        Result->SetArrayField(TEXT("nodes"), Nodes);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Graph details retrieved."), Result);
        return true;
    }

    SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown subAction: %s"), *SubAction), TEXT("INVALID_SUBACTION"));
    return true;
#else
    SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint graph actions are editor-only."), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

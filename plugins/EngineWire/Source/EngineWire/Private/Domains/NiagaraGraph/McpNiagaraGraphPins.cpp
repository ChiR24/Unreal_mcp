#include "Domains/NiagaraGraph/McpNiagaraGraphPrivate.h"

namespace McpNiagaraGraphHandlers
{
namespace
{
// autoConnect, between the two nodes the call names: an output of FromNode already wired into ToNode answers
// alreadyConnected, else the first output of FromNode and free input of ToNode the schema connects. It scanned the whole
// graph and answered alreadyConnected for the first wire anywhere, so in a real graph it connected nothing.
bool TryAutoConnectPins(
    UEngineWireSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UNiagaraSystem* System,
    UEdGraphNode* FromNode,
    UEdGraphNode* ToNode)
{
    auto Reply = [&](UEdGraphPin* FromPin, UEdGraphPin* ToPin, bool bAlreadyConnected)
    {
        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        if (!bAlreadyConnected)
        {
            SaveNiagaraGraphEdit(System, Payload, Result);
        }
        McpHandlerUtils::AddVerification(Result, System);
        Result->SetStringField(TEXT("fromNode"), FromNode->NodeGuid.ToString());
        Result->SetStringField(TEXT("fromPin"), FromPin->PinName.ToString());
        Result->SetStringField(TEXT("toNode"), ToNode->NodeGuid.ToString());
        Result->SetStringField(TEXT("toPin"), ToPin->PinName.ToString());
        Result->SetBoolField(TEXT("connected"), true);
        Result->SetBoolField(TEXT("autoConnected"), true);
        if (bAlreadyConnected)
        {
            Result->SetBoolField(TEXT("alreadyConnected"), true);
        }
        Bridge->SendAutomationResponse(Socket, RequestId, true,
            bAlreadyConnected ? TEXT("Pins were already connected.") : TEXT("Pins connected successfully."), Result);
        return true;
    };
    for (UEdGraphPin* FromPin : FromNode->Pins)
    {
        if (FromPin && FromPin->Direction == EGPD_Output)
        {
            for (UEdGraphPin* Linked : FromPin->LinkedTo)
            {
                if (Linked && Linked->GetOwningNode() == ToNode)
                {
                    return Reply(FromPin, Linked, true);
                }
            }
        }
    }
    // Free pins, linked plainly: a wired output would branch the parameter map, a wired input lose its wire (the schema
    // breaks it), and a conversion node is a node the call did not ask for.
    const UEdGraphSchema* Schema = FromNode->GetSchema();
    for (UEdGraphPin* FromPin : FromNode->Pins)
    {
        if (!FromPin || FromPin->Direction != EGPD_Output || FromPin->bHidden || !FromPin->LinkedTo.IsEmpty())
        {
            continue;
        }
        for (UEdGraphPin* ToPin : ToNode->Pins)
        {
            if (ToPin && ToPin->Direction == EGPD_Input && !ToPin->bHidden && ToPin->LinkedTo.IsEmpty() &&
                Schema->CanCreateConnection(FromPin, ToPin).Response == CONNECT_RESPONSE_MAKE &&
                Schema->TryCreateConnection(FromPin, ToPin))
            {
                return Reply(FromPin, ToPin, false);
            }
        }
    }
    Bridge->SendAutomationError(Socket, RequestId,
        TEXT("No free output of fromNode links directly to a free input of toNode; name fromPin and toPin."), TEXT("PIN_NOT_FOUND"));
    return true;
}
} // namespace

UEdGraphNode* FindNodeByIdNameOrTitle(UNiagaraGraph* TargetGraph, const FString& NodeRef)
{
    for (UEdGraphNode* Node : TargetGraph->Nodes)
    {
        if (!Node)
        {
            continue;
        }

        const FString NodeId = Node->NodeGuid.ToString();
        const FString NodeName = Node->GetName();
        const FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
        if (NodeId == NodeRef || NodeName == NodeRef || NodeTitle == NodeRef)
        {
            return Node;
        }
    }
    return nullptr;
}

UEdGraphPin* FindPinByNameOrDisplayName(UEdGraphNode* Node, const FString& PinName)
{
    if (UEdGraphPin* Pin = Node->FindPin(FName(*PinName)))
    {
        return Pin;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->GetDisplayName().ToString() == PinName)
        {
            return Pin;
        }
    }
    return nullptr;
}

bool HandleConnectPins(
    UEngineWireSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UNiagaraSystem* System,
    UNiagaraGraph* TargetGraph)
{
    FString FromNodeId, FromPinName;
    FString ToNodeId, ToPinName;
    const bool bAutoConnect = GetJsonBoolField(Payload, TEXT("autoConnect"), false);
    const bool bHasNodes = Payload->TryGetStringField(TEXT("fromNode"), FromNodeId) &&
        Payload->TryGetStringField(TEXT("toNode"), ToNodeId);
    const bool bHasPins = Payload->TryGetStringField(TEXT("fromPin"), FromPinName) &&
        Payload->TryGetStringField(TEXT("toPin"), ToPinName);
    if (!bHasNodes || (!bHasPins && !bAutoConnect))
    {
        Bridge->SendAutomationError(Socket, RequestId,
            TEXT("connect_pins requires fromNode, fromPin, toNode, toPin (with autoConnect, fromNode and toNode)"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UEdGraphNode* FromNode = FindNodeByIdNameOrTitle(TargetGraph, FromNodeId);
    UEdGraphNode* ToNode = FindNodeByIdNameOrTitle(TargetGraph, ToNodeId);
    if (!FromNode || !ToNode)
    {
        Bridge->SendAutomationError(Socket, RequestId,
            TEXT("Could not find source or destination node."), TEXT("NODE_NOT_FOUND"));
        return true;
    }
    if (!bHasPins)
    {
        return TryAutoConnectPins(Bridge, RequestId, Payload, Socket, System, FromNode, ToNode);
    }

    UEdGraphPin* FromPin = FindPinByNameOrDisplayName(FromNode, FromPinName);
    UEdGraphPin* ToPin = FindPinByNameOrDisplayName(ToNode, ToPinName);
    if (!FromPin || !ToPin)
    {
        Bridge->SendAutomationError(Socket, RequestId,
            TEXT("Could not find source or destination pin."), TEXT("PIN_NOT_FOUND"));
        return true;
    }

    if (!TargetGraph->GetSchema()->TryCreateConnection(FromPin, ToPin))
    {
        Bridge->SendAutomationError(Socket, RequestId,
            TEXT("Failed to connect pins (schema blocked connection)."),
            TEXT("CONNECTION_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    SaveNiagaraGraphEdit(System, Payload, Result);
    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("fromNode"), FromNode->NodeGuid.ToString());
    Result->SetStringField(TEXT("fromPin"), FromPin->PinName.ToString());
    Result->SetStringField(TEXT("toNode"), ToNode->NodeGuid.ToString());
    Result->SetStringField(TEXT("toPin"), ToPin->PinName.ToString());
    Result->SetBoolField(TEXT("connected"), true);

    Bridge->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Pins connected successfully."), Result);
    return true;
}
}

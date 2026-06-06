#include "McpAutomationBridge_NiagaraGraphHandlersPrivate.h"

#if WITH_EDITOR
namespace McpNiagaraGraphHandlers
{
namespace
{
bool TryAutoConnectPins(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UNiagaraSystem* System,
    UNiagaraGraph* TargetGraph)
{
    TargetGraph->Modify();
    for (UEdGraphNode* FromCandidate : TargetGraph->Nodes)
    {
        if (!FromCandidate)
        {
            continue;
        }

        for (UEdGraphPin* FromCandidatePin : FromCandidate->Pins)
        {
            if (!FromCandidatePin || FromCandidatePin->Direction != EGPD_Output)
            {
                continue;
            }

            if (!FromCandidatePin->LinkedTo.IsEmpty())
            {
                UEdGraphPin* ExistingToPin = FromCandidatePin->LinkedTo[0];
                TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
                McpHandlerUtils::AddVerification(Result, System);
                Result->SetStringField(
                    TEXT("fromNode"), FromCandidate->NodeGuid.ToString());
                Result->SetStringField(
                    TEXT("fromPin"), FromCandidatePin->PinName.ToString());
                Result->SetStringField(
                    TEXT("toNode"),
                    ExistingToPin->GetOwningNode()->NodeGuid.ToString());
                Result->SetStringField(
                    TEXT("toPin"), ExistingToPin->PinName.ToString());
                Result->SetBoolField(TEXT("connected"), true);
                Result->SetBoolField(TEXT("autoConnected"), true);
                Result->SetBoolField(TEXT("alreadyConnected"), true);

                Bridge->SendAutomationResponse(Socket, RequestId, true,
                    TEXT("Pins were already connected."), Result);
                return true;
            }
        }
    }

    for (UEdGraphNode* FromCandidate : TargetGraph->Nodes)
    {
        if (!FromCandidate)
        {
            continue;
        }

        for (UEdGraphPin* FromCandidatePin : FromCandidate->Pins)
        {
            if (!FromCandidatePin || FromCandidatePin->Direction != EGPD_Output)
            {
                continue;
            }
            for (UEdGraphNode* ToCandidate : TargetGraph->Nodes)
            {
                if (!ToCandidate || ToCandidate == FromCandidate)
                {
                    continue;
                }

                for (UEdGraphPin* ToCandidatePin : ToCandidate->Pins)
                {
                    if (!ToCandidatePin || ToCandidatePin->Direction != EGPD_Input)
                    {
                        continue;
                    }

                    if (TargetGraph->GetSchema()->TryCreateConnection(FromCandidatePin, ToCandidatePin))
                    {
                        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
                        McpHandlerUtils::AddVerification(Result, System);
                        Result->SetStringField(TEXT("fromNode"), FromCandidate->NodeGuid.ToString());
                        Result->SetStringField(TEXT("fromPin"), FromCandidatePin->PinName.ToString());
                        Result->SetStringField(TEXT("toNode"), ToCandidate->NodeGuid.ToString());
                        Result->SetStringField(TEXT("toPin"), ToCandidatePin->PinName.ToString());
                        Result->SetBoolField(TEXT("connected"), true);
                        Result->SetBoolField(TEXT("autoConnected"), true);

                        Bridge->SendAutomationResponse(Socket, RequestId, true,
                            TEXT("Pins connected successfully."), Result);
                        return true;
                    }
                }
            }
        }
    }

    Bridge->SendAutomationError(Socket, RequestId,
        TEXT("Could not find a compatible Niagara graph pin pair to auto-connect."),
        TEXT("PIN_NOT_FOUND"));
    return true;
}

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
    if (!Node)
    {
        return nullptr;
    }

    if (UEdGraphPin* Pin = Node->FindPin(FName(*PinName)))
    {
        return Pin;
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && (Pin->PinName.ToString() == PinName ||
            Pin->GetDisplayName().ToString() == PinName))
        {
            return Pin;
        }
    }
    return nullptr;
}
}

bool HandleConnectPins(
    UMcpAutomationBridgeSubsystem* Bridge,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket,
    UNiagaraSystem* System,
    UNiagaraGraph* TargetGraph)
{
    FString FromNodeId, FromPinName;
    FString ToNodeId, ToPinName;
    const bool bAutoConnect = GetJsonBoolField(Payload, TEXT("autoConnect"), false);

    if (!Payload->TryGetStringField(TEXT("fromNode"), FromNodeId) ||
        !Payload->TryGetStringField(TEXT("fromPin"), FromPinName) ||
        !Payload->TryGetStringField(TEXT("toNode"), ToNodeId) ||
        !Payload->TryGetStringField(TEXT("toPin"), ToPinName))
    {
        if (bAutoConnect)
        {
            return TryAutoConnectPins(Bridge, RequestId, Socket, System, TargetGraph);
        }

        Bridge->SendAutomationError(Socket, RequestId,
            TEXT("connect_pins requires fromNode, fromPin, toNode, toPin"),
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
    McpHandlerUtils::AddVerification(Result, System);
    Result->SetStringField(TEXT("fromNode"), FromNodeId);
    Result->SetStringField(TEXT("fromPin"), FromPinName);
    Result->SetStringField(TEXT("toNode"), ToNodeId);
    Result->SetStringField(TEXT("toPin"), ToPinName);
    Result->SetBoolField(TEXT("connected"), true);

    Bridge->SendAutomationResponse(Socket, RequestId, true,
        TEXT("Pins connected successfully."), Result);
    return true;
}
}
#endif

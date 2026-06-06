#include "McpAutomationBridge_PCGHandlersPrivate.h"

#if WITH_EDITOR && MCP_HAS_PCG
namespace McpPCGHandlers
{
FString GetNodeTitleString(UPCGNode* Node)
{
    if (!Node)
    {
        return FString();
    }
    if (Node->NodeTitle != NAME_None)
    {
        return Node->NodeTitle.ToString();
    }
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
    return Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString();
#else
    return Node->GetNodeTitle().ToString();
#endif
}

TSharedPtr<FJsonObject> BuildNodeResult(UPCGGraph* Graph, UPCGNode* Node, const FString& GraphPath)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("graphPath"), GraphPath);
    if (Node)
    {
        Result->SetStringField(TEXT("nodeId"), Node->GetName());
        Result->SetStringField(TEXT("nodeName"), Node->GetName());
        Result->SetStringField(TEXT("title"), GetNodeTitleString(Node));
        if (UPCGSettings* Settings = Node->GetSettings())
        {
            Result->SetStringField(TEXT("nodeType"), Settings->GetClass()->GetName());
        }
        McpHandlerUtils::AddVerification(Result, Node);
    }
    else
    {
        McpHandlerUtils::AddVerification(Result, Graph);
    }
    return Result;
}

UPCGNode* FindPCGNode(UPCGGraph* Graph, const FString& NodeId)
{
    if (!Graph || NodeId.IsEmpty())
    {
        return nullptr;
    }

    const FString Needle = NodeId.TrimStartAndEnd();
    if (Needle.Equals(TEXT("input"), ESearchCase::IgnoreCase) || Needle.Equals(TEXT("input_node"), ESearchCase::IgnoreCase))
    {
        return Graph->GetInputNode();
    }
    if (Needle.Equals(TEXT("output"), ESearchCase::IgnoreCase) || Needle.Equals(TEXT("output_node"), ESearchCase::IgnoreCase))
    {
        return Graph->GetOutputNode();
    }
    if (Needle.IsNumeric())
    {
        const int32 Index = FCString::Atoi(*Needle);
        const TArray<UPCGNode*>& Nodes = Graph->GetNodes();
        if (Index >= 0 && Index < Nodes.Num())
        {
            return Nodes[Index];
        }
    }

    const TArray<UPCGNode*>& Nodes = Graph->GetNodes();
    for (UPCGNode* Node : Nodes)
    {
        if (!Node)
        {
            continue;
        }
        if (Node->GetName().Equals(Needle, ESearchCase::IgnoreCase) || Node->GetPathName().Equals(Needle, ESearchCase::IgnoreCase))
        {
            return Node;
        }
        if (Node->NodeTitle != NAME_None && Node->NodeTitle.ToString().Equals(Needle, ESearchCase::IgnoreCase))
        {
            return Node;
        }
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
        if (Node->GetNodeTitle(EPCGNodeTitleType::ListView).ToString().Equals(Needle, ESearchCase::IgnoreCase) ||
            Node->GetNodeTitle(EPCGNodeTitleType::FullTitle).ToString().Equals(Needle, ESearchCase::IgnoreCase))
#else
        if (Node->GetNodeTitle().ToString().Equals(Needle, ESearchCase::IgnoreCase))
#endif
        {
            return Node;
        }
    }

    return nullptr;
}

FString DescribePCGPinLabels(const TArray<TObjectPtr<UPCGPin>>& Pins)
{
    TArray<FString> Labels;
    for (const TObjectPtr<UPCGPin>& Pin : Pins)
    {
        if (Pin)
        {
            Labels.Add(Pin->Properties.Label.ToString());
        }
    }

    return Labels.IsEmpty() ? TEXT("<none>") : FString::Join(Labels, TEXT(", "));
}

UPCGPin* GetPCGPinByLabel(UPCGNode* Node, bool bOutputPin, const FName& Label)
{
    if (!Node)
    {
        return nullptr;
    }

    return bOutputPin ? Node->GetOutputPin(Label) : Node->GetInputPin(Label);
}

bool TryResolvePCGPinLabel(UPCGNode* Node, bool bOutputPin, const FString& RequestedPinLabel, FName& OutPinLabel, FString& OutError)
{
    if (!Node)
    {
        OutError = TEXT("PCG node is invalid.");
        return false;
    }

    const TCHAR* PinKind = bOutputPin ? TEXT("output") : TEXT("input");
    const TArray<TObjectPtr<UPCGPin>>& Pins = bOutputPin ? Node->GetOutputPins() : Node->GetInputPins();
    if (!RequestedPinLabel.IsEmpty())
    {
        const FName RequestedPin(*RequestedPinLabel);
        if (GetPCGPinByLabel(Node, bOutputPin, RequestedPin))
        {
            OutPinLabel = RequestedPin;
            return true;
        }

        OutError = FString::Printf(TEXT("PCG node '%s' has no %s pin '%s'. Available %s pins: %s."),
            *Node->GetName(), PinKind, *RequestedPinLabel, PinKind, *DescribePCGPinLabels(Pins));
        return false;
    }

    const FName PreferredPin = bOutputPin ? PCGPinConstants::DefaultOutputLabel : PCGPinConstants::DefaultInputLabel;
    if (GetPCGPinByLabel(Node, bOutputPin, PreferredPin))
    {
        OutPinLabel = PreferredPin;
        return true;
    }

    for (const TObjectPtr<UPCGPin>& Pin : Pins)
    {
        if (Pin)
        {
            OutPinLabel = Pin->Properties.Label;
            return true;
        }
    }

    OutError = FString::Printf(TEXT("PCG node '%s' has no %s pins."), *Node->GetName(), PinKind);
    return false;
}

bool HasPCGEdge(const UPCGPin* SourcePin, const UPCGPin* TargetPin)
{
    if (!SourcePin || !TargetPin)
    {
        return false;
    }

    for (const TObjectPtr<UPCGEdge>& Edge : SourcePin->Edges)
    {
        if (Edge && Edge->IsValid() && Edge->GetOtherPin(SourcePin) == TargetPin)
        {
            return true;
        }
    }

    return false;
}

void ApplyNodeMetadata(UPCGNode* Node, const TSharedPtr<FJsonObject>& Payload)
{
    if (!Node || !Payload.IsValid())
    {
        return;
    }

    const FString Title = GetFirstStringField(Payload, {TEXT("nodeName"), TEXT("title"), TEXT("name")});
    if (!Title.IsEmpty())
    {
        Node->NodeTitle = FName(*Title);
    }

    double X = 0.0;
    double Y = 0.0;
    const bool bHasX = Payload->TryGetNumberField(TEXT("x"), X) || Payload->TryGetNumberField(TEXT("posX"), X);
    const bool bHasY = Payload->TryGetNumberField(TEXT("y"), Y) || Payload->TryGetNumberField(TEXT("posY"), Y);
    if (bHasX || bHasY)
    {
        Node->SetNodePosition(static_cast<int32>(X), static_cast<int32>(Y));
    }
}

bool SaveGraphIfRequested(UPCGGraph* Graph, bool bSave, bool& bOutSaved, FString& OutError)
{
    bOutSaved = false;
    if (!Graph)
    {
        OutError = TEXT("PCG graph is invalid.");
        return false;
    }

    Graph->PostEditChange();
    Graph->MarkPackageDirty();
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
    Graph->ForceNotificationForEditor(EPCGChangeType::Structural);
#elif ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
    Graph->ForceNotificationForEditor();
#else
    Graph->NotifyGraphChanged(EPCGChangeType::Structural);
#endif

    if (bSave)
    {
        bOutSaved = McpSafeAssetSave(Graph);
        if (!bOutSaved)
        {
            OutError = FString::Printf(TEXT("Failed to save PCG graph '%s'."), *Graph->GetPathName());
            return false;
        }
    }

    return true;
}
}
#endif

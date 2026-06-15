#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
namespace McpBlueprintGraphHandlers
{
static TSharedPtr<FJsonObject> MakePinSummary(UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject =
        McpHandlerUtils::CreateResultObject();
    PinObject->SetStringField(
        TEXT("pinName"),
        Pin->PinName.ToString());
    PinObject->SetStringField(
        TEXT("pinType"),
        Pin->PinType.PinCategory.ToString());
    PinObject->SetStringField(
        TEXT("direction"),
        Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
    if ((Pin->PinType.PinCategory == TEXT("object") ||
         Pin->PinType.PinCategory == TEXT("class") ||
         Pin->PinType.PinCategory == TEXT("struct")) &&
        Pin->PinType.PinSubCategoryObject.IsValid())
    {
        PinObject->SetStringField(
            TEXT("pinSubType"),
            Pin->PinType.PinSubCategoryObject->GetName());
    }

    TArray<TSharedPtr<FJsonValue>> LinkedTo;
    for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
    {
        if (LinkedPin && LinkedPin->GetOwningNode())
        {
            TSharedPtr<FJsonObject> Link =
                McpHandlerUtils::CreateResultObject();
            Link->SetStringField(
                TEXT("nodeId"),
                LinkedPin->GetOwningNode()->NodeGuid.ToString());
            Link->SetStringField(
                TEXT("pinName"),
                LinkedPin->PinName.ToString());
            LinkedTo.Add(MakeShared<FJsonValueObject>(Link));
        }
    }
    PinObject->SetArrayField(TEXT("linkedTo"), LinkedTo);
    return PinObject;
}

static bool GetNodes(FActionContext& Context)
{
    if (Context.SubAction != TEXT("get_nodes"))
    {
        return false;
    }

    TArray<TSharedPtr<FJsonValue>> Nodes;
    for (UEdGraphNode* Node : Context.TargetGraph->Nodes)
    {
        if (!Node)
        {
            continue;
        }

        TSharedPtr<FJsonObject> NodeObject =
            McpHandlerUtils::CreateResultObject();
        NodeObject->SetStringField(
            TEXT("nodeId"),
            Node->NodeGuid.ToString());
        NodeObject->SetStringField(TEXT("nodeName"), Node->GetName());
        NodeObject->SetStringField(
            TEXT("nodeType"),
            Node->GetClass()->GetName());
        NodeObject->SetStringField(
            TEXT("nodeTitle"),
            Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
        NodeObject->SetStringField(TEXT("comment"), Node->NodeComment);
        NodeObject->SetNumberField(TEXT("x"), Node->NodePosX);
        NodeObject->SetNumberField(TEXT("y"), Node->NodePosY);

        TArray<TSharedPtr<FJsonValue>> Pins;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin)
            {
                Pins.Add(
                    MakeShared<FJsonValueObject>(MakePinSummary(Pin)));
            }
        }
        NodeObject->SetArrayField(TEXT("pins"), Pins);
        Nodes.Add(MakeShared<FJsonValueObject>(NodeObject));
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetArrayField(TEXT("nodes"), Nodes);
    Result->SetStringField(
        TEXT("graphName"),
        Context.TargetGraph->GetName());
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(TEXT("Nodes retrieved."), Result);
    return true;
}

static bool GetGraphDetails(FActionContext& Context)
{
    if (Context.SubAction != TEXT("get_graph_details"))
    {
        return false;
    }

    // Opt-in: include each node's pins (with their linkedTo connections) so a
    // graph's exec/data flow can be read in one call instead of a per-node
    // get_node_details loop. Default output is unchanged.
    bool bIncludePins = false;
    if (Context.Payload.IsValid())
    {
        Context.Payload->TryGetBoolField(TEXT("includePins"), bIncludePins);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(
        TEXT("graphName"),
        Context.TargetGraph->GetName());

    TArray<TSharedPtr<FJsonValue>> Nodes;
    for (UEdGraphNode* Node : Context.TargetGraph->Nodes)
    {
        if (!Node)
        {
            continue;
        }

        TSharedPtr<FJsonObject> NodeObject =
            McpHandlerUtils::CreateResultObject();
        NodeObject->SetStringField(
            TEXT("nodeId"),
            Node->NodeGuid.ToString());
        NodeObject->SetStringField(TEXT("nodeName"), Node->GetName());
        NodeObject->SetStringField(
            TEXT("nodeTitle"),
            Node->GetNodeTitle(ENodeTitleType::ListView).ToString());

        if (bIncludePins)
        {
            TArray<TSharedPtr<FJsonValue>> Pins;
            for (UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin)
                {
                    Pins.Add(
                        MakeShared<FJsonValueObject>(MakePinSummary(Pin)));
                }
            }
            NodeObject->SetArrayField(TEXT("pins"), Pins);
        }

        Nodes.Add(MakeShared<FJsonValueObject>(NodeObject));
    }
    // nodeCount reflects the nodes actually emitted in "nodes" (null graph slots
    // are skipped in the loop above), so the count and the array always agree.
    Result->SetNumberField(TEXT("nodeCount"), Nodes.Num());
    Result->SetArrayField(TEXT("nodes"), Nodes);
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(TEXT("Graph details retrieved."), Result);
    return true;
}

bool HandleNodeQueryAction(FActionContext& Context)
{
    return GetNodes(Context) || GetGraphDetails(Context);
}
}
#else
namespace McpBlueprintGraphHandlers
{
bool HandleNodeQueryAction(FActionContext&)
{
    return false;
}
}
#endif

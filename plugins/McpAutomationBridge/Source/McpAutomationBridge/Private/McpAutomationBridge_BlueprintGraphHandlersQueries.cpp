#include "McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

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

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(
        TEXT("graphName"),
        Context.TargetGraph->GetName());
    Result->SetNumberField(
        TEXT("nodeCount"),
        Context.TargetGraph->Nodes.Num());

    TArray<TSharedPtr<FJsonValue>> Nodes;
    for (UEdGraphNode* Node : Context.TargetGraph->Nodes)
    {
        TSharedPtr<FJsonObject> NodeObject =
            McpHandlerUtils::CreateResultObject();
        NodeObject->SetStringField(
            TEXT("nodeId"),
            Node->NodeGuid.ToString());
        NodeObject->SetStringField(TEXT("nodeName"), Node->GetName());
        NodeObject->SetStringField(
            TEXT("nodeTitle"),
            Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
        Nodes.Add(MakeShared<FJsonValueObject>(NodeObject));
    }
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

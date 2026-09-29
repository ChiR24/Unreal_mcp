#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

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
            // The far node's title, so a chain reads without a call per hop.
            Link->SetStringField(
                TEXT("nodeTitle"),
                LinkedPin->GetOwningNode()->GetNodeTitle(ENodeTitleType::ListView).ToString());
            LinkedTo.Add(MakeShared<FJsonValueObject>(Link));
        }
    }
    PinObject->SetArrayField(TEXT("linkedTo"), LinkedTo);
    // Report the pin literal here too, exactly as the single-node pin view
    // does. Without it this view answered with the wiring only, so a caller
    // reading a whole graph could not tell a pin holding "L_Hub" from an
    // empty one, and the absence of the field read as "no value" -- which
    // sent a live debugging session down the wrong branch entirely.
    if (!Pin->DefaultValue.IsEmpty())
    {
        PinObject->SetStringField(TEXT("defaultValue"), Pin->DefaultValue);
    }
    else if (!Pin->DefaultTextValue.IsEmptyOrWhitespace())
    {
        PinObject->SetStringField(
            TEXT("defaultTextValue"),
            Pin->DefaultTextValue.ToString());
    }
    else if (Pin->DefaultObject)
    {
        PinObject->SetStringField(
            TEXT("defaultObjectPath"),
            Pin->DefaultObject->GetPathName());
    }
    return PinObject;
}

// A node matches a filter by its title or name, or by what its pins hold: a default value, a text
// default, or the path of a default object. A Create Widget node names the widget it builds only on
// its Class pin (/Game/UI/WBP_MainMenu.WBP_MainMenu_C), so a title-and-name filter of "Menu" found
// nothing. Case-insensitive and spaces ignored on the pin text, like the title.
static bool NodeMatchesGraphFilter(const UEdGraphNode* Node, const FString& Title, const FString& SquashedFilter)
{
    const auto Matches = [&SquashedFilter](const FString& Text)
    {
        return Text.Replace(TEXT(" "), TEXT("")).Contains(SquashedFilter);
    };
    if (Matches(Title) || Node->GetName().Contains(SquashedFilter))
    {
        return true;
    }
    for (const UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && (Matches(Pin->DefaultValue) || Matches(Pin->DefaultTextValue.ToString()) ||
                    (Pin->DefaultObject && Matches(Pin->DefaultObject->GetPathName()))))
        {
            return true;
        }
    }
    return false;
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
    // A large graph with pins overflows the response cap (290 nodes did), so a
    // caller narrows by title, name or a pin's default (filter) and pages
    // (offset/limit); totalCount and hasMore say what is left.
    bool bIncludePins = false;
    FString Filter;
    int32 Offset = 0;
    int32 Limit = 0;
    if (Context.Payload.IsValid())
    {
        Context.Payload->TryGetBoolField(TEXT("includePins"), bIncludePins);
        Context.Payload->TryGetStringField(TEXT("filter"), Filter);
        Context.Payload->TryGetNumberField(TEXT("offset"), Offset);
        Context.Payload->TryGetNumberField(TEXT("limit"), Limit);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(
        TEXT("graphName"),
        Context.TargetGraph->GetName());

    // Spaces are ignored on both sides: the editor labels a node "Spawn System
    // at Location" while its title here reads SpawnSystemAtLocation, and a
    // filter typed from the editor matched nothing.
    const FString SquashedFilter = Filter.Replace(TEXT(" "), TEXT(""));
    TArray<TSharedPtr<FJsonValue>> Nodes;
    int32 Matched = 0;
    for (UEdGraphNode* Node : Context.TargetGraph->Nodes)
    {
        if (!Node)
        {
            continue;
        }
        const FString Title = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
        if (!Filter.IsEmpty() && !NodeMatchesGraphFilter(Node, Title, SquashedFilter))
        {
            continue;
        }
        if (Matched++ < Offset || (Limit > 0 && Nodes.Num() >= Limit))
        {
            continue;
        }

        TSharedPtr<FJsonObject> NodeObject =
            McpHandlerUtils::CreateResultObject();
        NodeObject->SetStringField(
            TEXT("nodeId"),
            Node->NodeGuid.ToString());
        NodeObject->SetStringField(TEXT("nodeName"), Node->GetName());
        NodeObject->SetStringField(TEXT("nodeTitle"), Title);

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
    Result->SetNumberField(TEXT("totalCount"), Matched);
    Result->SetBoolField(TEXT("hasMore"), Offset + Nodes.Num() < Matched);
    Result->SetArrayField(TEXT("nodes"), Nodes);
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(TEXT("Graph details retrieved."), Result);
    return true;
}

bool HandleNodeQueryAction(FActionContext& Context)
{
    return GetGraphDetails(Context);
}
}

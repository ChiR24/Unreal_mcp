#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"

#include "EdGraphSchema_K2.h"
#include "K2Node_VariableGet.h"

namespace McpBlueprintGraphHandlers
{
// An enum, soft reference or interface pin names its sub-type too, not only an object, class or struct; an array, set
// or map pin read as its element type ("int"), the same as one value, and a map pin never named its value type.
void AddPinTypeFields(const TSharedPtr<FJsonObject>& PinObject, const FEdGraphPinType& PinType)
{
    if (PinType.PinSubCategoryObject.IsValid())
    {
        PinObject->SetStringField(TEXT("pinSubType"), PinType.PinSubCategoryObject->GetName());
    }
    if (PinType.IsContainer())
    {
        PinObject->SetStringField(TEXT("container"), PinType.IsArray() ? TEXT("array") : PinType.IsSet() ? TEXT("set") : TEXT("map"));
    }
    if (PinType.IsMap())
    {
        const FEdGraphTerminalType& Value = PinType.PinValueType;
        PinObject->SetStringField(TEXT("valueType"), Value.TerminalSubCategoryObject.IsValid()
            ? Value.TerminalSubCategoryObject->GetName() : Value.TerminalCategory.ToString());
    }
}

// A pin's literal, for a visible input nothing is wired to: the compiled graph never reads an output's default, a
// wired input's or a hidden self pin's (a library call's class default object), and a wired Size pin showing
// "defaultValue": "0.0" read as the value it used.
void AddPinDefaultFields(const TSharedPtr<FJsonObject>& PinObject, const UEdGraphPin& Pin)
{
    if (Pin.bHidden || Pin.Direction != EGPD_Input || Pin.LinkedTo.Num() > 0)
    {
        return;
    }
    if (!Pin.DefaultValue.IsEmpty())
    {
        PinObject->SetStringField(TEXT("defaultValue"), Pin.DefaultValue);
    }
    else if (!Pin.DefaultTextValue.IsEmptyOrWhitespace())
    {
        PinObject->SetStringField(TEXT("defaultTextValue"), Pin.DefaultTextValue.ToString());
    }
    else if (Pin.DefaultObject)
    {
        PinObject->SetStringField(TEXT("defaultObjectPath"), Pin.DefaultObject->GetPathName());
    }
}

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
    AddPinTypeFields(PinObject, Pin->PinType);

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
    AddPinDefaultFields(PinObject, *Pin);
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
        if (!Pin)
        {
            continue;
        }
        if (Matches(Pin->DefaultValue) || Matches(Pin->DefaultTextValue.ToString()) ||
            (Pin->DefaultObject && Matches(Pin->DefaultObject->GetPathName())))
        {
            return true;
        }
        // An input wired from a variable or component of that name: "Arm" finds the Set Relative Rotation that moves the
        // Arm component, so every writer of it can be found (a Tick that sets it each frame overrides the others).
        for (const UEdGraphPin* Linked : Pin->LinkedTo)
        {
            const UK2Node_VariableGet* Getter =
                Pin->Direction == EGPD_Input && Linked ? Cast<UK2Node_VariableGet>(Linked->GetOwningNode()) : nullptr;
            if (Getter && Matches(Getter->GetVarNameString()))
            {
                return true;
            }
        }
    }
    return false;
}

// A node that does nothing: a call no exec wire reaches, or a getter, math or other pure node whose result nothing
// reads. Deleting what read a node leaves it behind, and the compiler says nothing. A node that starts a chain (an
// event, an input action, a function entry) never counts; nor does one without pins (a comment).
static bool NodeDoesNothing(const UEdGraphNode* Node)
{
    bool bExecIn = false, bExecInLinked = false, bExecOut = false, bDataOut = false, bDataOutLinked = false;
    for (const UEdGraphPin* Pin : Node->Pins)
    {
        if (!Pin || Pin->bHidden)
        {
            continue;
        }
        const bool bExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
        const bool bLinked = Pin->LinkedTo.Num() > 0;
        bExecIn |= bExec && Pin->Direction == EGPD_Input;
        bExecInLinked |= bExec && Pin->Direction == EGPD_Input && bLinked;
        bExecOut |= bExec && Pin->Direction == EGPD_Output;
        bDataOut |= !bExec && Pin->Direction == EGPD_Output;
        bDataOutLinked |= !bExec && Pin->Direction == EGPD_Output && bLinked;
    }
    return bExecIn ? !bExecInLinked : (!bExecOut && bDataOut && !bDataOutLinked);
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
    bool bUnusedOnly = false;
    FString Filter;
    int32 Offset = 0;
    int32 Limit = 0;
    if (Context.Payload.IsValid())
    {
        Context.Payload->TryGetBoolField(TEXT("includePins"), bIncludePins);
        Context.Payload->TryGetBoolField(TEXT("unusedOnly"), bUnusedOnly);
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
        if ((!Filter.IsEmpty() && !NodeMatchesGraphFilter(Node, Title, SquashedFilter)) ||
            (bUnusedOnly && !NodeDoesNothing(Node)))
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
        // The layout in one read: placing or tidying nodes took one node read per position.
        NodeObject->SetNumberField(TEXT("x"), Node->NodePosX);
        NodeObject->SetNumberField(TEXT("y"), Node->NodePosY);

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

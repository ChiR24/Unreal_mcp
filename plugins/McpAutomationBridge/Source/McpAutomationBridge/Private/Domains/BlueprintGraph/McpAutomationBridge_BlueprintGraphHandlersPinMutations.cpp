#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#include "Domains/BlueprintGraph/PinMutations/McpAutomationBridge_BlueprintGraphPinLinkReport.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node_VariableSet.h"
#include "ScopedTransaction.h"

namespace McpBlueprintGraphHandlers
{
static bool ConnectPins(FActionContext& Context)
{
    if (Context.SubAction != TEXT("connect_pins"))
    {
        return false;
    }

    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Connect Blueprint Pins")));
    Context.Blueprint->Modify();
    Context.TargetGraph->Modify();

    const FString FromNodeId = McpGetFirstStringField(
        Context.Payload, {TEXT("fromNodeId"), TEXT("fromNode"),
                          TEXT("sourceNodeGuid"), TEXT("sourceNodeId"),
                          TEXT("sourceNode"), TEXT("nodeId")});
    const FString FromPinName = McpGetFirstStringField(
        Context.Payload, {TEXT("fromPinName"), TEXT("fromPin"),
                          TEXT("sourcePinName"), TEXT("sourcePin"),
                          TEXT("outputPin"), TEXT("sourceOutputPin"),
                          TEXT("pinName")});
    const FString ToNodeId = McpGetFirstStringField(
        Context.Payload, {TEXT("toNodeId"), TEXT("toNode"),
                          TEXT("targetNodeGuid"), TEXT("targetNodeId"),
                          TEXT("targetNode")});
    const FString ToPinName = McpGetFirstStringField(
        Context.Payload, {TEXT("toPinName"), TEXT("toPin"),
                          TEXT("targetPinName"), TEXT("targetPin"),
                          TEXT("inputPin")});

    UEdGraphNode* FromNode = Context.FindNode(FromNodeId);
    UEdGraphNode* ToNode = Context.FindNode(ToNodeId);
    if (!FromNode || !ToNode)
    {
        // Name the endpoint that missed: "source or target" left the caller
        // re-checking an id that was fine.
        Context.SendError(
            FString::Printf(TEXT("Could not find the %s node '%s' in this graph. Use a node's nodeGuid (an "
                                 "unambiguous prefix of 8+ hex characters also works) or its node name; "
                                 "inspect_graph info \"graph\" lists them."),
                            !FromNode ? TEXT("source") : TEXT("target"), !FromNode ? *FromNodeId : *ToNodeId),
            TEXT("NODE_NOT_FOUND"));
        return true;
    }

    FromNode->Modify();
    ToNode->Modify();
    if (FromNode->Pins.Num() == 0)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("connect_pins: FromNode '%s' has no pins, calling AllocateDefaultPins"),
            *FromNode->GetName());
        FromNode->AllocateDefaultPins();
    }
    if (ToNode->Pins.Num() == 0)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("connect_pins: ToNode '%s' has no pins, calling AllocateDefaultPins"),
            *ToNode->GetName());
        ToNode->AllocateDefaultPins();
    }

    UEdGraphPin* FromPin = Context.FindPin(FromNode, FromPinName);
    UEdGraphPin* ToPin = Context.FindPin(ToNode, ToPinName);
    if (!FromPin || !ToPin)
    {
        FString FromPinsList;
        FString ToPinsList;
        for (UEdGraphPin* Pin : FromNode->Pins)
        {
            if (Pin)
            {
                FromPinsList += Pin->PinName.ToString() + TEXT(", ");
            }
        }
        for (UEdGraphPin* Pin : ToNode->Pins)
        {
            if (Pin)
            {
                ToPinsList += Pin->PinName.ToString() + TEXT(", ");
            }
        }
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("connect_pins: FromNode '%s' pins: %s"),
            *FromNode->GetName(),
            *FromPinsList);
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("connect_pins: ToNode '%s' pins: %s"),
            *ToNode->GetName(),
            *ToPinsList);
        // The pin lists were already computed - they just went to the editor
        // log, where the caller cannot see them.
        Context.SendError(
            FString::Printf(
                TEXT("No %s pin matched. FromNode '%s' pins: %s. ToNode '%s' pins: %s."),
                FromPin == nullptr ? TEXT("source") : TEXT("target"),
                *FromNode->GetName(), *FromPinsList,
                *ToNode->GetName(), *ToPinsList),
            TEXT("PIN_NOT_FOUND"));
        return true;
    }

    // A Set node's value pin carries the variable's name but is its INPUT. Named as the source of a
    // link into another input it can only mean the node's one output of that type (Output_Get).
    if (FromPin->Direction == EGPD_Input && ToPin->Direction == EGPD_Input && FromNode->IsA<UK2Node_VariableSet>())
    {
        TArray<UEdGraphPin*> SameTypeOutputs;
        for (UEdGraphPin* Pin : FromNode->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Output && Pin->PinType == FromPin->PinType) SameTypeOutputs.Add(Pin);
        }
        if (SameTypeOutputs.Num() == 1) FromPin = SameTypeOutputs[0];
    }

    // Snapshot node GUIDs so we can detect an auto-inserted conversion node.
    // When the pin types differ but an autocast exists, the schema silently
    // spawns a conversion node (e.g. real->int Truncate) inside
    // TryCreateConnection and still returns true. Surfacing it stops the caller
    // from unknowingly accepting a precision/semantic change.
    TSet<FGuid> NodeGuidsBefore;
    NodeGuidsBefore.Reserve(Context.TargetGraph->Nodes.Num());
    for (UEdGraphNode* ExistingNode : Context.TargetGraph->Nodes)
    {
        if (ExistingNode)
        {
            NodeGuidsBefore.Add(ExistingNode->NodeGuid);
        }
    }

    // A pin that holds one link (an exec output, a data input) drops its old one without a word.
    const TArray<UEdGraphPin*> FromBefore = FromPin->LinkedTo;
    const TArray<UEdGraphPin*> ToBefore = ToPin->LinkedTo;
    const UEdGraphSchema* Schema = Context.TargetGraph->GetSchema();
    if (!Schema->TryCreateConnection(FromPin, ToPin))
    {
        // Say why, and where the node's outputs are: "schema rejection" alone left
        // a caller who named a Set node's value INPUT as the source guessing, when
        // its output is Output_Get.
        const FPinConnectionResponse Response = Schema->CanCreateConnection(FromPin, ToPin);
        FString Outputs;
        for (const UEdGraphPin* Pin : FromNode->Pins)
        {
            if (Pin && Pin->Direction == EGPD_Output) Outputs += (Outputs.IsEmpty() ? TEXT("") : TEXT(", ")) + Pin->PinName.ToString();
        }
        Context.SendError(
            FString::Printf(TEXT("Cannot connect %s (%s %s) to %s (%s %s): %s. Outputs of '%s': %s."),
                *FromPin->PinName.ToString(), FromPin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"),
                *FromPin->PinType.PinCategory.ToString(), *ToPin->PinName.ToString(),
                ToPin->Direction == EGPD_Input ? TEXT("input") : TEXT("output"), *ToPin->PinType.PinCategory.ToString(),
                Response.Message.IsEmpty() ? TEXT("the graph schema refused the link") : *Response.Message.ToString(),
                *FromNode->GetName(), Outputs.IsEmpty() ? TEXT("none") : *Outputs),
            TEXT("CONNECTION_FAILED"));
        return true;
    }

    // A node present now but not before == the auto-inserted conversion node.
    UEdGraphNode* ConversionNode = nullptr;
    for (UEdGraphNode* MaybeNew : Context.TargetGraph->Nodes)
    {
        if (MaybeNew && !NodeGuidsBefore.Contains(MaybeNew->NodeGuid))
        {
            ConversionNode = MaybeNew;
            break;
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Context.Blueprint);
    SaveLoadedAssetThrottled(Context.Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (ConversionNode)
    {
        Result->SetBoolField(TEXT("conversionInserted"), true);
        Result->SetStringField(
            TEXT("conversionNodeId"), ConversionNode->NodeGuid.ToString());
        Result->SetStringField(
            TEXT("conversionNodeTitle"),
            ConversionNode->GetNodeTitle(ENodeTitleType::ListView).ToString());
        Result->SetStringField(
            TEXT("note"),
            TEXT("Pin types differed; an automatic conversion node was inserted "
                 "between the pins (possible precision/semantic change)."));
    }
    // "Pins connected." with an identical dataDigest for every call was no
    // evidence at all: a caller could not tell which pins were linked, or
    // whether the link survived. Read it back off the graph.
    Result->SetBoolField(TEXT("connected"), FromPin->LinkedTo.Contains(ToPin));
    Result->SetStringField(TEXT("sourcePinName"), FromPin->GetName());
    Result->SetStringField(TEXT("targetPinName"), ToPin->GetName());
    Result->SetStringField(TEXT("sourcePinType"), FromPin->PinType.PinCategory.ToString());
    Result->SetStringField(TEXT("targetPinType"), ToPin->PinType.PinCategory.ToString());
    McpReportPinLinkChanges(Result, FromPin, ToPin, FromBefore, ToBefore);
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(
        ConversionNode ? TEXT("Pins connected (conversion node inserted).")
                       : FString::Printf(TEXT("Connected %s -> %s."),
                                         *FromPin->GetName(), *ToPin->GetName()),
        Result);
    return true;
}

static bool BreakPinLinks(FActionContext& Context)
{
    if (Context.SubAction != TEXT("break_pin_links"))
    {
        return false;
    }

    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Break Blueprint Pin Links")));
    Context.Blueprint->Modify();
    Context.TargetGraph->Modify();

    const FString NodeId = McpGetFirstStringField(
        Context.Payload, {TEXT("nodeId"), TEXT("nodeGuid"), TEXT("fromNodeId"),
                          TEXT("fromNode"), TEXT("sourceNodeGuid"),
                          TEXT("sourceNodeId"), TEXT("sourceNode")});
    const FString PinName = McpGetFirstStringField(
        Context.Payload, {TEXT("pinName"), TEXT("pin"), TEXT("fromPinName"),
                          TEXT("fromPin"), TEXT("sourcePinName"),
                          TEXT("sourcePin"), TEXT("sourceOutputPin")});
    UEdGraphNode* TargetNode = Context.FindNode(NodeId);
    if (!TargetNode)
    {
        Context.SendNodeNotFound(NodeId);
        return true;
    }

    UEdGraphPin* Pin = Context.FindPin(TargetNode, PinName);
    if (!Pin)
    {
        Context.SendError(
            FString::Printf(TEXT("No pin named '%s'. Pins on this node: %s."),
                *PinName, *DescribeNodePins(TargetNode)),
            TEXT("PIN_NOT_FOUND"));
        return true;
    }

    TargetNode->Modify();
    Context.TargetGraph->GetSchema()->BreakPinLinks(*Pin, true);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Context.Blueprint);
    SaveLoadedAssetThrottled(Context.Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(TEXT("Pin links broken."), Result);
    return true;
}

bool HandlePinMutationAction(FActionContext& Context)
{
    return ConnectPins(Context) ||
           BreakPinLinks(Context) ||
           SetPinDefaultValue(Context);
}
}

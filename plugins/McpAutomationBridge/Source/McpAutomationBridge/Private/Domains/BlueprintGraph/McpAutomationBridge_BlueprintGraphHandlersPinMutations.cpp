#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "EdGraph/EdGraphSchema.h"
#include "ScopedTransaction.h"

namespace McpBlueprintGraphHandlers
{
// Accept the documented node/pin field-name aliases. These were added to
// HandleBlueprintConnectPins, but that is not the handler connect_pins
// reaches: the action routes to ConnectPins below, and this path read only
// the from*/to* names — so every documented sourceNodeId/sourceNode/nodeId
// call failed with NODE_NOT_FOUND. break_pin_links had the same gap against
// its own nodeId/pinName pair. In every case the canonical name is listed
// first, so existing callers keep their exact precedence.
FString PickFirstNonEmpty(const TSharedPtr<FJsonObject>& Payload,
                                 const TArray<const TCHAR*>& Keys)
{
    FString Value;
    for (const TCHAR* Key : Keys)
    {
        if (Payload->TryGetStringField(Key, Value) && !Value.IsEmpty())
        {
            return Value;
        }
    }
    return FString();
}

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

    const FString FromNodeId = PickFirstNonEmpty(
        Context.Payload, {TEXT("fromNodeId"), TEXT("fromNode"),
                          TEXT("sourceNodeGuid"), TEXT("sourceNodeId"),
                          TEXT("sourceNode"), TEXT("nodeId")});
    const FString FromPinName = PickFirstNonEmpty(
        Context.Payload, {TEXT("fromPinName"), TEXT("fromPin"),
                          TEXT("sourcePinName"), TEXT("sourcePin"),
                          TEXT("outputPin"), TEXT("sourceOutputPin"),
                          TEXT("pinName")});
    const FString ToNodeId = PickFirstNonEmpty(
        Context.Payload, {TEXT("toNodeId"), TEXT("toNode"),
                          TEXT("targetNodeGuid"), TEXT("targetNodeId"),
                          TEXT("targetNode")});
    const FString ToPinName = PickFirstNonEmpty(
        Context.Payload, {TEXT("toPinName"), TEXT("toPin"),
                          TEXT("targetPinName"), TEXT("targetPin"),
                          TEXT("inputPin")});

    UEdGraphNode* FromNode = Context.FindNode(FromNodeId);
    UEdGraphNode* ToNode = Context.FindNode(ToNodeId);
    if (!FromNode || !ToNode)
    {
        Context.SendError(
            TEXT("Could not find source or target node."),
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
        Context.SendError(
            TEXT("Could not find source or target pin."),
            TEXT("PIN_NOT_FOUND"));
        return true;
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

    if (!Context.TargetGraph->GetSchema()->TryCreateConnection(
            FromPin,
            ToPin))
    {
        Context.SendError(
            TEXT("Failed to connect pins (schema rejection)."),
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
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(
        ConversionNode ? TEXT("Pins connected (conversion node inserted).")
                       : TEXT("Pins connected."),
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

    const FString NodeId = PickFirstNonEmpty(
        Context.Payload, {TEXT("nodeId"), TEXT("nodeGuid"), TEXT("fromNodeId"),
                          TEXT("fromNode"), TEXT("sourceNodeGuid"),
                          TEXT("sourceNodeId"), TEXT("sourceNode")});
    const FString PinName = PickFirstNonEmpty(
        Context.Payload, {TEXT("pinName"), TEXT("pin"), TEXT("fromPinName"),
                          TEXT("fromPin"), TEXT("sourcePinName"),
                          TEXT("sourcePin"), TEXT("sourceOutputPin")});
    UEdGraphNode* TargetNode = Context.FindNode(NodeId);
    if (!TargetNode)
    {
        Context.SendError(TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        return true;
    }

    UEdGraphPin* Pin = Context.FindPin(TargetNode, PinName);
    if (!Pin)
    {
        Context.SendError(TEXT("Pin not found."), TEXT("PIN_NOT_FOUND"));
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
#else
namespace McpBlueprintGraphHandlers
{
bool HandlePinMutationAction(FActionContext&)
{
    return false;
}
}
#endif

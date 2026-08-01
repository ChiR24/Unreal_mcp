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
static FString PickFirstNonEmpty(const TSharedPtr<FJsonObject>& Payload,
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

    if (!Context.TargetGraph->GetSchema()->TryCreateConnection(
            FromPin,
            ToPin))
    {
        Context.SendError(
            TEXT("Failed to connect pins (schema rejection)."),
            TEXT("CONNECTION_FAILED"));
        return true;
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Context.Blueprint);
    SaveLoadedAssetThrottled(Context.Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(TEXT("Pins connected."), Result);
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

static bool SetPinDefaultValue(FActionContext& Context)
{
    if (Context.SubAction != TEXT("set_pin_default_value"))
    {
        return false;
    }

    const FString NodeId = PickFirstNonEmpty(
        Context.Payload, {TEXT("nodeId"), TEXT("nodeGuid"), TEXT("toNodeId"),
                          TEXT("toNode"), TEXT("targetNodeGuid"),
                          TEXT("targetNodeId"), TEXT("targetNode")});
    const FString PinName = PickFirstNonEmpty(
        Context.Payload, {TEXT("pinName"), TEXT("pin"), TEXT("targetPinName"),
                          TEXT("targetPin"), TEXT("inputPin")});
    FString Value;
    Context.Payload->TryGetStringField(TEXT("value"), Value);

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
    if (Pin->Direction != EGPD_Input)
    {
        Context.SendError(
            TEXT("Can only set default values on input pins."),
            TEXT("INVALID_PIN_DIRECTION"));
        return true;
    }

    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Set Pin Default Value")));
    Context.Blueprint->Modify();
    Context.TargetGraph->Modify();
    TargetNode->Modify();
    Context.TargetGraph->GetSchema()->TrySetDefaultValue(*Pin, Value);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Context.Blueprint);
    SaveLoadedAssetThrottled(Context.Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), NodeId);
    Result->SetStringField(TEXT("nodeName"), TargetNode->GetName());
    Result->SetStringField(TEXT("pinName"), PinName);
    Result->SetStringField(TEXT("value"), Value);
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(TEXT("Pin default value set."), Result);
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

#include "McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "EdGraph/EdGraphSchema.h"
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

    FString FromNodeId;
    FString FromPinName;
    FString ToNodeId;
    FString ToPinName;
    Context.Payload->TryGetStringField(TEXT("fromNodeId"), FromNodeId);
    Context.Payload->TryGetStringField(TEXT("fromPinName"), FromPinName);
    Context.Payload->TryGetStringField(TEXT("toNodeId"), ToNodeId);
    Context.Payload->TryGetStringField(TEXT("toPinName"), ToPinName);

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

    FString NodeId;
    FString PinName;
    Context.Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    Context.Payload->TryGetStringField(TEXT("pinName"), PinName);
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

    FString NodeId;
    FString PinName;
    FString Value;
    Context.Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    Context.Payload->TryGetStringField(TEXT("pinName"), PinName);
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

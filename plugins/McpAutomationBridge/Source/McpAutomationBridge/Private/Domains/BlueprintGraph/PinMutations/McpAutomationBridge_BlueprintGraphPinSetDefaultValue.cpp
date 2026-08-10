// Blueprint pin-mutation: SetPinDefaultValue handler, split from
// McpAutomationBridge_BlueprintGraphHandlersPinMutations.cpp.
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "EdGraph/EdGraphSchema.h"
#include "ScopedTransaction.h"

namespace McpBlueprintGraphHandlers
{
bool SetPinDefaultValue(FActionContext& Context)
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
}
#endif

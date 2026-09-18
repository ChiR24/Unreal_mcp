#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "K2Node_Knot.h"
#include "ScopedTransaction.h"

namespace McpBlueprintGraphHandlers
{
static bool DeleteNode(FActionContext& Context)
{
    if (Context.SubAction != TEXT("delete_node"))
    {
        return false;
    }

    FString NodeId;
    Context.Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    UEdGraphNode* TargetNode = Context.FindNode(NodeId);
    if (!TargetNode)
    {
        Context.SendError(TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        return true;
    }

    // `pinName` only means something to the break_pin_links fold, which the
    // caller selects with deleteScope "pin_links". Sent without it, the request
    // says "operate on this pin" and the default scope says "delete the whole
    // node" -- and the node wins, silently. That cost a working Branch node and
    // the death branch hanging off it. A destructive default must not resolve a
    // contradiction in its own favour: refuse and name the scope that does what
    // the pin was clearly meant to do.
    FString ScopedPinName;
    if (Context.Payload->TryGetStringField(TEXT("pinName"), ScopedPinName) &&
        !ScopedPinName.IsEmpty())
    {
        Context.SendError(
            FString::Printf(
                TEXT("'pinName' ('%s') was sent with deleteScope 'node', which deletes "
                     "the ENTIRE node and ignores the pin. Re-send with "
                     "deleteScope: \"pin_links\" to break that pin's links instead, or "
                     "drop 'pinName' to confirm you meant to delete the whole node."),
                *ScopedPinName),
            TEXT("CONTRADICTORY_SCOPE"));
        return true;
    }

    // Honor the node's own deletability (the same gate the editor UI uses).
    // Removing structural roots like K2Node_FunctionEntry leaves the function
    // graph orphaned; a later compile then hits an engine check() and fatally
    // crashes the editor (see ReplaceFunctionReferences, NAME_None assert).
    if (!TargetNode->CanUserDeleteNode())
    {
        Context.SendError(
            FString::Printf(
                TEXT("Node '%s' (%s) is not user-deletable — removing it would corrupt "
                     "the graph (function entry/result nodes are managed by the editor)."),
                *TargetNode->GetName(), *TargetNode->GetClass()->GetName()),
            TEXT("PROTECTED_NODE"));
        return true;
    }

    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Delete Blueprint Node")));
    Context.Blueprint->Modify();
    Context.TargetGraph->Modify();
    FBlueprintEditorUtils::RemoveNode(
        Context.Blueprint,
        TargetNode,
        true);
    SaveLoadedAssetThrottled(Context.Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(TEXT("Node deleted."), Result);
    return true;
}

static bool CreateRerouteNode(FActionContext& Context)
{
    if (Context.SubAction != TEXT("create_reroute_node"))
    {
        return false;
    }

    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Create Reroute Node")));
    Context.Blueprint->Modify();
    Context.TargetGraph->Modify();

    float X = 0.0f;
    float Y = 0.0f;
    // Match create_node: accept the tool-facing posX/posY names, which reach the
    // native transport unnormalized (the TS bridge's posX->x mapping is bypassed).
    if (!Context.Payload->TryGetNumberField(TEXT("x"), X))
    {
        Context.Payload->TryGetNumberField(TEXT("posX"), X);
    }
    if (!Context.Payload->TryGetNumberField(TEXT("y"), Y))
    {
        Context.Payload->TryGetNumberField(TEXT("posY"), Y);
    }

    FGraphNodeCreator<UK2Node_Knot> NodeCreator(*Context.TargetGraph);
    UK2Node_Knot* RerouteNode = NodeCreator.CreateNode(false);
    RerouteNode->NodePosX = X;
    RerouteNode->NodePosY = Y;
    NodeCreator.Finalize();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Context.Blueprint);
    SaveLoadedAssetThrottled(Context.Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(
        TEXT("nodeId"),
        RerouteNode->NodeGuid.ToString());
    // The contract requires nodeGuid; nodeId is kept for older callers.
    Result->SetStringField(TEXT("nodeGuid"), RerouteNode->NodeGuid.ToString());
    Result->SetStringField(
        TEXT("nodeName"),
        RerouteNode->GetName());
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(TEXT("Reroute node created."), Result);
    return true;
}

static bool SetNodeProperty(FActionContext& Context)
{
    if (Context.SubAction != TEXT("set_node_property"))
    {
        return false;
    }

    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Set Blueprint Node Property")));
    Context.Blueprint->Modify();
    Context.TargetGraph->Modify();

    FString NodeId;
    FString PropertyName;
    FString Value;
    Context.Payload->TryGetStringField(TEXT("nodeId"), NodeId);
    Context.Payload->TryGetStringField(
        TEXT("propertyName"),
        PropertyName);
    Context.Payload->TryGetStringField(TEXT("value"), Value);

    UEdGraphNode* TargetNode = Context.FindNode(NodeId);
    if (!TargetNode)
    {
        Context.SendError(TEXT("Node not found."), TEXT("NODE_NOT_FOUND"));
        return true;
    }

    TargetNode->Modify();
    bool bHandled = false;
    if (PropertyName.Equals(TEXT("Comment"), ESearchCase::IgnoreCase) ||
        PropertyName.Equals(
            TEXT("NodeComment"),
            ESearchCase::IgnoreCase))
    {
        TargetNode->NodeComment = Value;
        bHandled = true;
    }
    else if (
        PropertyName.Equals(TEXT("X"), ESearchCase::IgnoreCase) ||
        PropertyName.Equals(TEXT("NodePosX"), ESearchCase::IgnoreCase))
    {
        double NumberValue = 0.0;
        if (!Context.Payload->TryGetNumberField(
                TEXT("value"),
                NumberValue))
        {
            NumberValue = FCString::Atod(*Value);
        }
        TargetNode->NodePosX = static_cast<float>(NumberValue);
        bHandled = true;
    }
    else if (
        PropertyName.Equals(TEXT("Y"), ESearchCase::IgnoreCase) ||
        PropertyName.Equals(TEXT("NodePosY"), ESearchCase::IgnoreCase))
    {
        double NumberValue = 0.0;
        if (!Context.Payload->TryGetNumberField(
                TEXT("value"),
                NumberValue))
        {
            NumberValue = FCString::Atod(*Value);
        }
        TargetNode->NodePosY = static_cast<float>(NumberValue);
        bHandled = true;
    }
    else if (PropertyName.Equals(
                 TEXT("bCommentBubbleVisible"),
                 ESearchCase::IgnoreCase))
    {
        TargetNode->bCommentBubbleVisible = Value.ToBool();
        bHandled = true;
    }
    else if (PropertyName.Equals(
                 TEXT("bCommentBubblePinned"),
                 ESearchCase::IgnoreCase))
    {
        TargetNode->bCommentBubblePinned = Value.ToBool();
        bHandled = true;
    }
    else if (
        PropertyName.Equals(TEXT("EnabledState"), ESearchCase::IgnoreCase) ||
        PropertyName.Equals(TEXT("bDisabled"), ESearchCase::IgnoreCase))
    {
        // Enable/disable a node (BUG-d870cf: the set was previously comment/position-only). "bDisabled" takes a
        // bool; "EnabledState" also accepts the enum names Enabled / Disabled / DevelopmentOnly.
        ENodeEnabledState NewState = ENodeEnabledState::Enabled;
        if (PropertyName.Equals(TEXT("bDisabled"), ESearchCase::IgnoreCase))
        {
            NewState = Value.ToBool()
                           ? ENodeEnabledState::Disabled
                           : ENodeEnabledState::Enabled;
        }
        else if (Value.Equals(TEXT("Enabled"), ESearchCase::IgnoreCase))
        {
            NewState = ENodeEnabledState::Enabled;
        }
        else if (Value.Equals(TEXT("Disabled"), ESearchCase::IgnoreCase))
        {
            NewState = ENodeEnabledState::Disabled;
        }
        else if (Value.Equals(
                     TEXT("DevelopmentOnly"),
                     ESearchCase::IgnoreCase))
        {
            NewState = ENodeEnabledState::DevelopmentOnly;
        }
        else
        {
            // Reject an unrecognized EnabledState string instead of silently treating it as Enabled, so a typo
            // (e.g. "Disable") is reported rather than leaving the node in the wrong state under a success reply.
            Context.SendError(
                FString::Printf(
                    TEXT("Invalid EnabledState '%s' (expected Enabled, Disabled, or DevelopmentOnly)"),
                    *Value),
                TEXT("INVALID_ARGUMENT"));
            return true;
        }
        TargetNode->SetEnabledState(NewState);
        bHandled = true;
    }

    if (!bHandled)
    {
        // Name the supported set: every other rejection in this tool lists its
        // allowed values, and without them a caller cannot tell whether the
        // property is spelled wrong or simply not settable here.
        Context.SendError(
            FString::Printf(
                TEXT("Unsupported node property '%s' (supported: comment, ")
                TEXT("NodePosX/X, NodePosY/Y, bCommentBubbleVisible, ")
                TEXT("bCommentBubblePinned, EnabledState, bDisabled). A node is ")
                TEXT("moved by setting NodePosX and NodePosY; node-class fields ")
                TEXT("such as a cast target or subsystem type are set when the ")
                TEXT("node is created, via create_node targetClass."),
                *PropertyName),
            TEXT("PROPERTY_NOT_SUPPORTED"));
        return true;
    }

    Context.TargetGraph->NotifyGraphChanged();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Context.Blueprint);
    SaveLoadedAssetThrottled(Context.Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(
        TEXT("nodeId"),
        TargetNode->NodeGuid.ToString());
    Result->SetStringField(TEXT("nodeName"), TargetNode->GetName());
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(TEXT("Node property updated."), Result);
    return true;
}

bool HandleNodeMutationAction(FActionContext& Context)
{
    return DeleteNode(Context) ||
           CreateRerouteNode(Context) ||
           SetNodeProperty(Context);
}
}
#else
namespace McpBlueprintGraphHandlers
{
bool HandleNodeMutationAction(FActionContext&)
{
    return false;
}
}
#endif

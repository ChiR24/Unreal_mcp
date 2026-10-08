#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"
#include "Domains/BlueprintGraph/Expression/McpAutomationBridge_BlueprintGraphMathExpression.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

#include "K2Node_Knot.h"
#include "ScopedTransaction.h"

namespace McpBlueprintGraphHandlers
{
// delete_node and arrange_nodes (both take nodeIds) live in ...BlueprintGraphHandlersDeleteNodes.cpp.
bool DeleteNodes(FActionContext& Context);
bool ArrangeNodes(FActionContext& Context);

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
    // posX/posY are the declared names, so they win over the legacy x/y.
    if (!Context.Payload->TryGetNumberField(TEXT("posX"), X))
    {
        Context.Payload->TryGetNumberField(TEXT("x"), X);
    }
    if (!Context.Payload->TryGetNumberField(TEXT("posY"), Y))
    {
        Context.Payload->TryGetNumberField(TEXT("y"), Y);
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

    // The contract declares propertyValue (any scalar); only `value` used to be
    // read, which the gateway refuses, so every call wrote an empty value
    // (comment cleared, position 0) and reported success. `value` stays for the
    // build_graph steps that still send it.
    TSharedPtr<FJsonValue> ValueField = Context.Payload->TryGetField(TEXT("propertyValue"));
    if (!ValueField.IsValid())
    {
        ValueField = Context.Payload->TryGetField(TEXT("value"));
    }
    FString Value;
    if (!McpJsonScalarToString(ValueField, Value))
    {
        Context.SendError(
            TEXT("propertyValue is required: a string, number or boolean (e.g. \"Entry point\" for NodeComment, 320 for NodePosX)."),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Set Blueprint Node Property")));
    Context.Blueprint->Modify();
    Context.TargetGraph->Modify();

    const FString NodeId = McpGetFirstStringField(Context.Payload, {TEXT("nodeId"), TEXT("nodeGuid")});
    FString PropertyName;
    Context.Payload->TryGetStringField(
        TEXT("propertyName"),
        PropertyName);

    UEdGraphNode* TargetNode = Context.FindNode(NodeId);
    if (!TargetNode)
    {
        Context.SendNodeNotFound(NodeId);
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
        TargetNode->NodePosX = static_cast<float>(FCString::Atod(*Value));
        bHandled = true;
    }
    else if (
        PropertyName.Equals(TEXT("Y"), ESearchCase::IgnoreCase) ||
        PropertyName.Equals(TEXT("NodePosY"), ESearchCase::IgnoreCase))
    {
        TargetNode->NodePosY = static_cast<float>(FCString::Atod(*Value));
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

    // Anything else may still be a reflected field on the node or on its
    // FAnimNode_* payload -- that is how an AnimGraph player is told which
    // Sequence or BlendSpace to play.
    // A math expression that does not parse ("-1": there is no unary minus) rebuilt into a node with
    // no output and answered success; the caller only found out when a later connect failed.
    const bool bMathExpression = PropertyName.Equals(TEXT("Expression"), ESearchCase::IgnoreCase) &&
        TargetNode->GetClass()->GetName() == TEXT("K2Node_MathExpression");
    const FStrProperty* ExpressionProp = bMathExpression ? FindFProperty<FStrProperty>(TargetNode->GetClass(), TEXT("Expression")) : nullptr;
    const FString OldExpression = ExpressionProp ? ExpressionProp->GetPropertyValue_InContainer(TargetNode) : FString();
    // The failed parse drops the input links, so the restore below puts them back by pin name.
    TArray<TPair<FName, TArray<UEdGraphPin*>>> SavedLinks;
    for (const UEdGraphPin* Pin : bMathExpression ? TargetNode->Pins : TArray<UEdGraphPin*>())
    {
        if (Pin && !Pin->bOrphanedPin && Pin->LinkedTo.Num() > 0) SavedLinks.Emplace(Pin->PinName, Pin->LinkedTo);
    }
    if (!bHandled)
        bHandled = McpTrySetNodeAssetPropertyForMcp(TargetNode, PropertyName, Value);
    // An orphaned pin is the old output kept only for its links: counting it hid the failed parse.
    if (bHandled && bMathExpression && !TargetNode->Pins.ContainsByPredicate([](const UEdGraphPin* Pin)
        { return Pin && Pin->Direction == EGPD_Output && !Pin->bOrphanedPin; }))
    {
        McpTrySetNodeAssetPropertyForMcp(TargetNode, PropertyName, OldExpression);
        for (const TPair<FName, TArray<UEdGraphPin*>>& Saved : SavedLinks)
        {
            UEdGraphPin* Pin = TargetNode->FindPin(Saved.Key);
            for (UEdGraphPin* Other : Saved.Value)
            {
                if (Pin && Other && !Pin->LinkedTo.Contains(Other)) Pin->MakeLinkTo(Other);
            }
        }
        Context.TargetGraph->NotifyGraphChanged();
        FString Unknown = McpBlueprintMathExpression::DescribeUnknownFunctions(Value);
        if (!Unknown.IsEmpty())
        {
            Unknown += TEXT(" ");
        }
        Context.SendError(
            FString::Printf(TEXT("The expression '%s' does not parse, so the node would have no output; it keeps the "
                                 "old one. %sCall a function by its compact title (max, clamp, abs, sin), else its name "
                                 "(Fraction, SelectFloat); write a negative number as 0 - x; FInterpTo is a CallFunction "
                                 "node, not an expression."),
                *Value, *Unknown),
            TEXT("EXPRESSION_INVALID"));
        return true;
    }

    if (!bHandled)
    {
        // Name the supported set: every other rejection in this tool lists its
        // allowed values, and without them a caller cannot tell whether the
        // property is spelled wrong or simply not settable here.
        Context.SendError(
            FString::Printf(
                TEXT("Unsupported node property '%s' (supported: comment, ")
                TEXT("NodePosX (or X), NodePosY (or Y), bCommentBubbleVisible, ")
                TEXT("bCommentBubblePinned, EnabledState, bDisabled, plus any ")
                TEXT("reflected node field such as an AnimGraph player's ")
                TEXT("Sequence or BlendSpace, set by asset path)."),
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
    if (bMathExpression)
    {
        McpBlueprintMathExpression::DescribeInputs(Context.Blueprint, *TargetNode, Value, Result);
    }
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(TEXT("Node property updated."), Result);
    return true;
}

bool HandleNodeMutationAction(FActionContext& Context)
{
    return DeleteNodes(Context) ||
           CreateRerouteNode(Context) ||
           SetNodeProperty(Context) ||
           ArrangeNodes(Context);
}
}

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
    Context.Payload->TryGetNumberField(TEXT("x"), X);
    Context.Payload->TryGetNumberField(TEXT("y"), Y);

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

    if (!bHandled)
    {
        Context.SendError(
            FString::Printf(
                TEXT("Unsupported node property '%s'"),
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

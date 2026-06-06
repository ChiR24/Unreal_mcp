#include "McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"

namespace McpBlueprintGraphHandlers
{
bool HandleNodeCreationAction(FActionContext& Context)
{
    if (Context.SubAction != TEXT("create_node"))
    {
        return false;
    }

    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Create Blueprint Node")));
    Context.Blueprint->Modify();
    Context.TargetGraph->Modify();

    FString NodeType;
    Context.Payload->TryGetStringField(TEXT("nodeType"), NodeType);
    float X = 0.0f;
    float Y = 0.0f;
    Context.Payload->TryGetNumberField(TEXT("x"), X);
    Context.Payload->TryGetNumberField(TEXT("y"), Y);

    if (TryCreateCommonFunctionNode(Context, NodeType, X, Y) ||
        TryCreateVariableNode(Context, NodeType, X, Y) ||
        TryCreateFunctionOrEventNode(Context, NodeType, X, Y) ||
        TryCreateCustomEventNode(Context, NodeType, X, Y) ||
        TryCreateSpecialNode(Context, NodeType, X, Y))
    {
        return true;
    }

    CreateDynamicNode(Context, NodeType, X, Y);
    return true;
}

void CreateDynamicNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    UClass* NodeClass = FindNodeClassByName(NodeType);
    if (!NodeClass)
    {
        Context.SendError(
            FString::Printf(
                TEXT("Node type '%s' not found. Use list_node_types to see available types."),
                *NodeType),
            TEXT("NODE_TYPE_NOT_FOUND"));
        return;
    }

    if (TryCreateEnhancedInputNode(Context, NodeClass, X, Y))
    {
        return;
    }

    UEdGraphNode* NewNode =
        NewObject<UEdGraphNode>(Context.TargetGraph, NodeClass);
    if (!NewNode)
    {
        Context.SendError(
            TEXT("Failed to instantiate node."),
            TEXT("CREATE_FAILED"));
        return;
    }

    Context.TargetGraph->AddNode(NewNode, false, false);
    NewNode->CreateNewGuid();
    NewNode->PostPlacedNewNode();
    NewNode->AllocateDefaultPins();
    NewNode->NodePosX = X;
    NewNode->NodePosY = Y;
    FBlueprintEditorUtils::MarkBlueprintAsModified(Context.Blueprint);
    SaveLoadedAssetThrottled(Context.Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
    Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
    Result->SetStringField(TEXT("nodeClass"), NodeClass->GetName());
    Context.SendResponse(TEXT("Node created."), Result);
}
}
#else
namespace McpBlueprintGraphHandlers
{
bool HandleNodeCreationAction(FActionContext&)
{
    return false;
}
}
#endif

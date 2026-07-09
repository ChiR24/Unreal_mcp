#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "EdGraph/EdGraphSchema.h"
#include "InputAction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_InputAxisEvent.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintGraphHandlers
{
bool TryCreateSpecialNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    if (NodeType == TEXT("Cast") ||
        NodeType.StartsWith(TEXT("CastTo")))
    {
        FString TargetClassName;
        Context.Payload->TryGetStringField(
            TEXT("targetClass"),
            TargetClassName);
        if (TargetClassName.IsEmpty() &&
            NodeType.StartsWith(TEXT("CastTo")))
        {
            TargetClassName = NodeType.Mid(6);
        }
        UClass* TargetClass = ResolveUClass(TargetClassName);
        if (!TargetClass)
        {
            Context.SendError(
                FString::Printf(
                    TEXT("Class '%s' not found"),
                    *TargetClassName),
                TEXT("CLASS_NOT_FOUND"));
            return true;
        }

        FGraphNodeCreator<UK2Node_DynamicCast> NodeCreator(
            *Context.TargetGraph);
        UK2Node_DynamicCast* Node = NodeCreator.CreateNode(false);
        Node->TargetType = TargetClass;
        Context.FinalizeNode(NodeCreator, Node, X, Y);
        return true;
    }

    if (NodeType != TEXT("InputAxisEvent") &&
        NodeType != TEXT("K2Node_InputAxisEvent"))
    {
        return false;
    }

    FString InputAxisName;
    Context.Payload->TryGetStringField(
        TEXT("inputAxisName"),
        InputAxisName);
    if (InputAxisName.IsEmpty())
    {
        Context.SendError(
            TEXT("inputAxisName required"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FGraphNodeCreator<UK2Node_InputAxisEvent> NodeCreator(
        *Context.TargetGraph);
    UK2Node_InputAxisEvent* Node = NodeCreator.CreateNode(false);
    Node->Initialize(FName(*InputAxisName));
    Context.FinalizeNode(NodeCreator, Node, X, Y);
    return true;
}

bool TryCreateEnhancedInputNode(
    FActionContext& Context,
    UClass* NodeClass,
    float X,
    float Y)
{
    if (!NodeClass->GetName().Equals(
            TEXT("K2Node_EnhancedInputAction"),
            ESearchCase::IgnoreCase))
    {
        return false;
    }

    FString InputActionPath;
    Context.Payload->TryGetStringField(
        TEXT("inputActionPath"),
        InputActionPath);
    if (InputActionPath.IsEmpty())
    {
        Context.Payload->TryGetStringField(
            TEXT("inputActionAssetPath"),
            InputActionPath);
    }
    if (InputActionPath.IsEmpty())
    {
        Context.Payload->TryGetStringField(
            TEXT("actionPath"),
            InputActionPath);
    }
    if (InputActionPath.IsEmpty())
    {
        Context.SendError(
            TEXT("actionPath or inputActionPath required"),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    int32 DotIndex = INDEX_NONE;
    FString CleanActionPath = InputActionPath;
    FString PackagePath = CleanActionPath;
    if (CleanActionPath.FindChar(TEXT('.'), DotIndex))
    {
        PackagePath = CleanActionPath.Left(DotIndex);
    }
    const FString SanitizedPackagePath =
        SanitizeProjectRelativePath(PackagePath);
    if (SanitizedPackagePath.IsEmpty())
    {
        Context.SendError(
            TEXT("Invalid input action path"),
            TEXT("INVALID_PATH"));
        return true;
    }
    CleanActionPath = DotIndex == INDEX_NONE
        ? SanitizedPackagePath
        : SanitizedPackagePath + CleanActionPath.Mid(DotIndex);

    UInputAction* InputAction =
        LoadObject<UInputAction>(nullptr, *CleanActionPath);
    if (!InputAction && !CleanActionPath.Contains(TEXT(".")))
    {
        const FString AssetName =
            FPackageName::GetShortName(CleanActionPath);
        const FString ObjectPath = FString::Printf(
            TEXT("%s.%s"),
            *CleanActionPath,
            *AssetName);
        InputAction = LoadObject<UInputAction>(nullptr, *ObjectPath);
        if (InputAction)
        {
            CleanActionPath = ObjectPath;
        }
    }
    if (!InputAction)
    {
        Context.SendError(
            FString::Printf(
                TEXT("Input action not found: %s"),
                *InputActionPath),
            TEXT("ASSET_NOT_FOUND"));
        return true;
    }

    UEdGraphNode* NewNode =
        NewObject<UEdGraphNode>(Context.TargetGraph, NodeClass);
    if (!NewNode)
    {
        Context.SendError(
            TEXT("Failed to instantiate node."),
            TEXT("CREATE_FAILED"));
        return true;
    }

    FObjectProperty* InputActionProperty = CastField<FObjectProperty>(
        NewNode->GetClass()->FindPropertyByName(
            FName(TEXT("InputAction"))));
    if (!InputActionProperty)
    {
        Context.SendError(
            TEXT("Enhanced Input node has no writable InputAction property."),
            TEXT("PROPERTY_NOT_FOUND"));
        return true;
    }

    Context.TargetGraph->AddNode(NewNode, false, false);
    NewNode->Modify();
    InputActionProperty->SetObjectPropertyValue_InContainer(
        NewNode,
        InputAction);
    NewNode->CreateNewGuid();
    NewNode->PostPlacedNewNode();
    // Guard against duplicate pins: some node types already allocate in
    // PostPlacedNewNode(), so only allocate when the node has no pins yet.
    if (NewNode->Pins.Num() == 0) { NewNode->AllocateDefaultPins(); }
    if (UK2Node* K2Node = Cast<UK2Node>(NewNode))
    {
        K2Node->ReconstructNode();
    }
    NewNode->NodePosX = X;
    NewNode->NodePosY = Y;
    if (const UEdGraphSchema* Schema =
            Context.TargetGraph->GetSchema())
    {
        Schema->ForceVisualizationCacheClear();
    }
    Context.TargetGraph->NotifyGraphChanged();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Context.Blueprint);
    SaveLoadedAssetThrottled(Context.Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
    Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
    Result->SetStringField(TEXT("nodeClass"), NodeClass->GetName());
    Result->SetStringField(TEXT("inputActionPath"), CleanActionPath);
    McpHandlerUtils::AddVerification(Result, Context.Blueprint);
    Context.SendResponse(TEXT("Enhanced Input node created."), Result);
    return true;
}
}
#endif

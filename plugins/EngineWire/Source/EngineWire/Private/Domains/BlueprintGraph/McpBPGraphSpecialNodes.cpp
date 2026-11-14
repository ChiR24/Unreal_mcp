#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"

#include "EdGraph/EdGraphSchema.h"
#include "InputAction.h"
#include "K2Node_ConstructObjectFromClass.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_InputAxisEvent.h"
#include "Misc/PackageName.h"
#include "UObject/UnrealType.h"

namespace McpBlueprintGraphHandlers
{
// Read the requested target class for a node with a class pin. Checks
// targetClass first, then the legacy memberClass / nodeClass / widgetType
// fields, then peels a "CastTo<Class>" prefix from the nodeType as a final
// fallback. Returns empty string if nothing was supplied.
static FString ReadTargetClassPayload(
    const FActionContext& Context,
    const FString& NodeType)
{
    FString TargetClass;
    Context.Payload->TryGetStringField(TEXT("targetClass"), TargetClass);
    if (TargetClass.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("memberClass"), TargetClass);
    }
    if (TargetClass.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("nodeClass"), TargetClass);
    }
    if (TargetClass.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("widgetType"), TargetClass);
    }
    if (TargetClass.IsEmpty() &&
        NodeType.StartsWith(TEXT("CastTo"), ESearchCase::IgnoreCase))
    {
        TargetClass = NodeType.Mid(6);
    }
    return TargetClass;
}

void CreateCastNode(FActionContext& Context, const FString& NodeType, float X, float Y)
{
    // TargetType must be set, or the node renders as an unusable "Bad cast node" (wildcard Object pin, no typed
    // "As <Class>" output). A Blueprint asset path (/Game/X/BP_Door) names its generated class.
    const FString TargetClass = ReadTargetClassPayload(Context, NodeType);
    if (TargetClass.IsEmpty())
    {
        Context.SendError(
            TEXT("DynamicCast node requires a 'targetClass' (Blueprint asset "
                 "path like /Game/Blueprints/BP_Cole, or a class name)."),
            TEXT("INVALID_ARGUMENT"));
        return;
    }
    UClass* ResolvedTarget = ResolveClassByName(TargetClass);
    if (!ResolvedTarget)
    {
        Context.SendError(
            FString::Printf(
                TEXT("Could not resolve targetClass '%s' for DynamicCast."),
                *TargetClass),
            TEXT("CLASS_NOT_FOUND"));
        return;
    }

    FGraphNodeCreator<UK2Node_DynamicCast> NodeCreator(*Context.TargetGraph);
    UK2Node_DynamicCast* Node = NodeCreator.CreateNode(false);
    Node->TargetType = ResolvedTarget;
    // Impure by default (the exec-driven Cast To... node); pure gives a data-only cast with no exec pins.
    bool bPureCast = false;
    Context.Payload->TryGetBoolField(TEXT("pure"), bPureCast);
    Node->SetPurity(bPureCast);
    Context.FinalizeNode(NodeCreator, Node, X, Y);
}

bool TryCreateSpecialNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    if (NodeType == TEXT("Cast") ||
        NodeType.StartsWith(TEXT("CastTo")))
    {
        CreateCastNode(Context, NodeType, X, Y);
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
    // The whole input-node family shares a writable `InputAction` object
    // property. Binding it is what separates a usable node from a silent
    // "Event None" / "InputAction None" husk with no value pins: the generic
    // path below never sets it, so nodes created that way looked successful
    // but could never fire.
    const FString NodeClassName = NodeClass->GetName();
    const bool bEnhancedEvent = NodeClassName.Equals(
        TEXT("K2Node_EnhancedInputActionEvent"), ESearchCase::IgnoreCase);
    const bool bInputFamily =
        NodeClassName.Equals(TEXT("K2Node_EnhancedInputAction"), ESearchCase::IgnoreCase) ||
        bEnhancedEvent ||
        NodeClassName.Equals(TEXT("K2Node_InputAction"), ESearchCase::IgnoreCase);
    if (!bInputFamily)
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
            McpPathRefusalMessage(TEXT("input action path"), PackagePath),
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
    // Allocate pins BEFORE PostPlacedNewNode(): checked pin accessors inside
    // PostPlacedNewNode() assert when the pin list is still empty (EdGraphNode.h:586).
    if (NewNode->Pins.Num() == 0) { NewNode->AllocateDefaultPins(); }
    NewNode->PostPlacedNewNode();
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
    TSharedPtr<FJsonObject> Fields = McpHandlerUtils::CreateResultObject();
    Fields->SetStringField(TEXT("nodeClass"), NodeClass->GetName());
    Fields->SetStringField(TEXT("inputActionPath"), CleanActionPath);
    Context.FinishCreatedNode(NewNode, X, Y, Fields);
    return true;
}
}

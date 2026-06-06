#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleBlueprintNodeValueActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("set_anim_graph_node_value"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("blueprintPath"), TEXT("")));
        FString NodeName = GetStringFieldAnimAuth(Params, TEXT("nodeName"), TEXT(""));
        FString PropertyName = GetStringFieldAnimAuth(Params, TEXT("propertyName"), TEXT(""));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        if (NodeName.IsEmpty() || PropertyName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("nodeName and propertyName are required"), TEXT("MISSING_PARAMETERS"));
        }

        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }

#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }

        // Find the node by name (search both NodeTitle and NodeComment)
        UEdGraphNode* FoundNode = nullptr;
        for (UEdGraphNode* Node : AnimGraph->Nodes)
        {
            if (Node)
            {
                // Check NodeComment first (custom name set via add_blend_node)
                if (Node->NodeComment.Contains(NodeName))
                {
                    FoundNode = Node;
                    break;
                }
                // Also check the node title
                if (Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Contains(NodeName))
                {
                    FoundNode = Node;
                    break;
                }
            }
        }

        if (!FoundNode)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Node '%s' not found in AnimGraph"), *NodeName), TEXT("NODE_NOT_FOUND"));
        }

        // Find and set the property using reflection
        // Support dot notation for nested properties (e.g., "BlendNode.Alpha")
        void* TargetContainer = FoundNode;
        FProperty* Property = nullptr;
        FString RemainingPath = PropertyName;

        while (!RemainingPath.IsEmpty())
        {
            FString CurrentPart;
            int32 DotIndex;
            if (RemainingPath.FindChar(TEXT('.'), DotIndex))
            {
                CurrentPart = RemainingPath.Left(DotIndex);
                RemainingPath = RemainingPath.Mid(DotIndex + 1);
            }
            else
            {
                CurrentPart = RemainingPath;
                RemainingPath.Empty();
            }

            FProperty* CurrentProp = TargetContainer
                ? FoundNode->GetClass()->FindPropertyByName(FName(*CurrentPart))
                : nullptr;

            // If searching on a struct, use the struct's property lookup
            if (!CurrentProp && Property)
            {
                if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
                {
                    CurrentProp = StructProp->Struct->FindPropertyByName(FName(*CurrentPart));
                    if (CurrentProp)
                    {
                        TargetContainer = Property->ContainerPtrToValuePtr<void>(TargetContainer);
                        Property = CurrentProp;
                        continue;
                    }
                }
            }

            if (!CurrentProp)
            {
                ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Property '%s' not found on node '%s'"), *CurrentPart, *NodeName), TEXT("PROPERTY_NOT_FOUND"));
            }

            Property = CurrentProp;
        }

        if (!Property)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Property '%s' not found on node '%s'"), *PropertyName, *NodeName), TEXT("PROPERTY_NOT_FOUND"));
        }

        // Get the value from params and apply it
        TSharedPtr<FJsonValue> ValueField = Params->TryGetField(TEXT("value"));
        if (!ValueField.IsValid())
        {
            ANIM_ERROR_RESPONSE(TEXT("value parameter is required"), TEXT("MISSING_VALUE"));
        }

        FString ApplyError;
        if (!ApplyJsonValueToProperty(FoundNode, Property, ValueField, ApplyError))
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Failed to set property: %s"), *ApplyError), TEXT("PROPERTY_SET_FAILED"));
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);

        Response->SetStringField(TEXT("nodeName"), NodeName);
        Response->SetStringField(TEXT("propertyName"), PropertyName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Property '%s' set on node '%s'"), *PropertyName, *NodeName));
#else
        // AnimGraph headers not available - return error
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot set node value on '%s': AnimGraph module headers not available in this build."), *NodeName),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
    }

    // ===== 10.5 Control Rig =====
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR

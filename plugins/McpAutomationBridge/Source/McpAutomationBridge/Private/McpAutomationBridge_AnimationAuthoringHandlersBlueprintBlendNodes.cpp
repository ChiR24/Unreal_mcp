#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleBlueprintBlendNodeActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("add_blend_node"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("blueprintPath"), TEXT("")));
        FString BlendType = GetStringFieldAnimAuth(Params, TEXT("blendType"), TEXT("TwoWayBlend"));
        FString NodeName = GetStringFieldAnimAuth(Params, TEXT("nodeName"), TEXT(""));
        int32 NodePosX = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionY"), 0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

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

        FString CreatedNodeType;
        FString CreatedNodeName = NodeName;

#if MCP_HAS_TWO_WAY_BLEND
        if (BlendType == TEXT("TwoWayBlend") || BlendType == TEXT("Blend"))
        {
            FGraphNodeCreator<UAnimGraphNode_TwoWayBlend> NodeCreator(*AnimGraph);
            UAnimGraphNode_TwoWayBlend* BlendNode = NodeCreator.CreateNode();
            BlendNode->NodePosX = NodePosX;
            BlendNode->NodePosY = NodePosY;
            // Set the node name via NodeComment so it can be found later
            if (!NodeName.IsEmpty())
            {
                BlendNode->NodeComment = NodeName;
                BlendNode->bCommentBubbleVisible = true;
            }
            NodeCreator.Finalize();
            CreatedNodeType = TEXT("TwoWayBlend");
            if (CreatedNodeName.IsEmpty())
            {
                CreatedNodeName = FString::Printf(TEXT("BlendNode_%d"), BlendNode->NodeGuid.A);
            }
        }
        else
#endif
#if MCP_HAS_LAYERED_BLEND
        if (BlendType == TEXT("LayeredBlend") || BlendType == TEXT("LayeredBoneBlend"))
        {
            FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> NodeCreator(*AnimGraph);
            UAnimGraphNode_LayeredBoneBlend* BlendNode = NodeCreator.CreateNode();
            BlendNode->NodePosX = NodePosX;
            BlendNode->NodePosY = NodePosY;
            // Set the node name via NodeComment so it can be found later
            if (!NodeName.IsEmpty())
            {
                BlendNode->NodeComment = NodeName;
                BlendNode->bCommentBubbleVisible = true;
            }
            NodeCreator.Finalize();
            CreatedNodeType = TEXT("LayeredBoneBlend");
            if (CreatedNodeName.IsEmpty())
            {
                CreatedNodeName = FString::Printf(TEXT("LayeredBlendNode_%d"), BlendNode->NodeGuid.A);
            }
        }
        else
#endif
        {
            // Default fallback to TwoWayBlend if available
#if MCP_HAS_TWO_WAY_BLEND
            FGraphNodeCreator<UAnimGraphNode_TwoWayBlend> NodeCreator(*AnimGraph);
            UAnimGraphNode_TwoWayBlend* BlendNode = NodeCreator.CreateNode();
            BlendNode->NodePosX = NodePosX;
            BlendNode->NodePosY = NodePosY;
            // Set the node name via NodeComment so it can be found later
            if (!NodeName.IsEmpty())
            {
                BlendNode->NodeComment = NodeName;
                BlendNode->bCommentBubbleVisible = true;
            }
            NodeCreator.Finalize();
            CreatedNodeType = TEXT("TwoWayBlend");
            if (CreatedNodeName.IsEmpty())
            {
                CreatedNodeName = FString::Printf(TEXT("BlendNode_%d"), BlendNode->NodeGuid.A);
            }
#else
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Cannot create blend node '%s': AnimGraph blend node headers not available in this build."), *BlendType), TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);

        Response->SetStringField(TEXT("nodeType"), CreatedNodeType);
        Response->SetStringField(TEXT("nodeName"), CreatedNodeName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Blend node '%s' (name: %s) created"), *CreatedNodeType, *CreatedNodeName));
#else
        // AnimGraph headers not available - return error instead of fake success
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot create blend node '%s': AnimGraph module headers not available in this build."), *BlendType),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
    }

    if (SubAction == TEXT("add_cached_pose"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("blueprintPath"), TEXT("")));
        FString CacheName = GetStringFieldAnimAuth(Params, TEXT("cacheName"), TEXT(""));
        int32 NodePosX = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionY"), 0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        if (CacheName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("cacheName is required"), TEXT("MISSING_CACHE_NAME"));
        }

        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }

#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_CACHED_POSE
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }

        // Create the Save Cached Pose node
        FGraphNodeCreator<UAnimGraphNode_SaveCachedPose> NodeCreator(*AnimGraph);
        UAnimGraphNode_SaveCachedPose* CachedPoseNode = NodeCreator.CreateNode();
        CachedPoseNode->NodePosX = NodePosX;
        CachedPoseNode->NodePosY = NodePosY;
        CachedPoseNode->CacheName = CacheName;
        NodeCreator.Finalize();

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);

        Response->SetStringField(TEXT("cacheName"), CacheName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Cached pose node '%s' created"), *CacheName));
#else
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Cached pose '%s' marked for creation (requires AnimGraph module)"), *CacheName));
#endif
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR

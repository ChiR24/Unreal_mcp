#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_BlendListByInt.h"

namespace McpAnimationAuthoring {

namespace
{
// A blend node of type TNode at (X, Y); Comment names it so it can be found later.
template <typename TNode>
UAnimGraphNode_Base* CreateMcpBlendNode(UEdGraph& Graph, int32 X, int32 Y, const FString& Comment)
{
    FGraphNodeCreator<TNode> NodeCreator(Graph);
    TNode* Node = NodeCreator.CreateNode();
    Node->NodePosX = X;
    Node->NodePosY = Y;
    if (!Comment.IsEmpty())
    {
        Node->NodeComment = Comment;
        Node->bCommentBubbleVisible = true;
    }
    NodeCreator.Finalize();
    return Node;
}
}

TSharedPtr<FJsonObject> HandleBlueprintBlendNodeActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("add_blend_node"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("blueprintPath"), TEXT("")));
        FString BlendType = GetJsonStringField(Params, TEXT("blendType"), TEXT("TwoWayBlend"));
        FString NodeName = GetJsonStringField(Params, TEXT("nodeName"), TEXT(""));
        int32 NodePosX = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionY"), 0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }

        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }

        // BlendListByBool and BlendListByInt are documented types, yet anything
        // but a layered blend fell through to a TwoWayBlend and reported success.
        FString CreatedNodeType;
        UAnimGraphNode_Base* BlendNode = nullptr;
        if (BlendType == TEXT("LayeredBlend") || BlendType == TEXT("LayeredBoneBlend"))
        {
            BlendNode = CreateMcpBlendNode<UAnimGraphNode_LayeredBoneBlend>(*AnimGraph, NodePosX, NodePosY, NodeName);
            CreatedNodeType = TEXT("LayeredBoneBlend");
        }
        else if (BlendType == TEXT("TwoWayBlend"))
        {
            BlendNode = CreateMcpBlendNode<UAnimGraphNode_TwoWayBlend>(*AnimGraph, NodePosX, NodePosY, NodeName);
            CreatedNodeType = TEXT("TwoWayBlend");
        }
        else if (BlendType == TEXT("BlendListByBool"))
        {
            BlendNode = CreateMcpBlendNode<UAnimGraphNode_BlendListByBool>(*AnimGraph, NodePosX, NodePosY, NodeName);
            CreatedNodeType = TEXT("BlendListByBool");
        }
        else if (BlendType == TEXT("BlendListByInt"))
        {
            BlendNode = CreateMcpBlendNode<UAnimGraphNode_BlendListByInt>(*AnimGraph, NodePosX, NodePosY, NodeName);
            CreatedNodeType = TEXT("BlendListByInt");
        }
        else
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Unknown blendType '%s'; use TwoWayBlend, BlendListByBool, BlendListByInt or LayeredBoneBlend"), *BlendType), TEXT("UNKNOWN_BLEND_TYPE"));
        }
        const FString CreatedNodeName = !NodeName.IsEmpty() ? NodeName
            : FString::Printf(TEXT("%s_%d"), BlendType.StartsWith(TEXT("Layered")) ? TEXT("LayeredBlendNode") : TEXT("BlendNode"), BlendNode->NodeGuid.A);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);

        Response->SetStringField(TEXT("nodeType"), CreatedNodeType);
        Response->SetStringField(TEXT("nodeName"), CreatedNodeName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Blend node '%s' (name: %s) created"), *CreatedNodeType, *CreatedNodeName));
        return Response;
    }

    if (SubAction == TEXT("add_cached_pose"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("blueprintPath"), TEXT("")));
        FString CacheName = GetJsonStringField(Params, TEXT("cacheName"), TEXT(""));
        int32 NodePosX = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionY"), 0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        if (CacheName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("cacheName is required"), TEXT("MISSING_CACHE_NAME"));
        }

        UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }

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
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring

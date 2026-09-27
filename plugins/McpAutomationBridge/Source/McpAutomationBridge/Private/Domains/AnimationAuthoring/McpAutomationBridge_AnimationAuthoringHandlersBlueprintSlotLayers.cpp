#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"

namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleBlueprintSlotLayerActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("add_slot_node"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("blueprintPath"), TEXT("")));
        FString SlotName = GetJsonStringField(Params, TEXT("slotName"), TEXT(""));
        FString GroupName = GetJsonStringField(Params, TEXT("groupName"), TEXT("DefaultGroup"));
        int32 NodePosX = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetJsonNumberField(Params, TEXT("positionY"), 0));
        bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

        if (SlotName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("slotName is required"), TEXT("MISSING_SLOT_NAME"));
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

        // Create the Slot node
        FGraphNodeCreator<UAnimGraphNode_Slot> NodeCreator(*AnimGraph);
        UAnimGraphNode_Slot* SlotNode = NodeCreator.CreateNode();
        SlotNode->NodePosX = NodePosX;
        SlotNode->NodePosY = NodePosY;

        // Set the slot name (format: "GroupName.SlotName")
        FString FullSlotName = FString::Printf(TEXT("%s.%s"), *GroupName, *SlotName);
        SlotNode->Node.SlotName = FName(*FullSlotName);

        NodeCreator.Finalize();

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);

        Response->SetStringField(TEXT("slotName"), FullSlotName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Slot node '%s' created"), *FullSlotName));
        return Response;
    }

    if (SubAction == TEXT("add_layered_blend_per_bone"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetJsonStringField(Params, TEXT("blueprintPath"), TEXT("")));
        FString BoneName = GetJsonStringField(Params, TEXT("boneName"), TEXT(""));
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

        // layerSetup (or the boneName shorthand) used to be read and dropped: the
        // node came out with no branch filters, which blends nothing, and the
        // call still reported success. Parse and check every bone first.
        TArray<FInputBlendPose> Layers;
        const TArray<TSharedPtr<FJsonValue>>* LayerValues = nullptr;
        if (Params->TryGetArrayField(TEXT("layerSetup"), LayerValues) && LayerValues)
        {
            for (const TSharedPtr<FJsonValue>& LayerValue : *LayerValues)
            {
                const TSharedPtr<FJsonObject>* LayerObj = nullptr;
                const TArray<TSharedPtr<FJsonValue>>* Filters = nullptr;
                if (!LayerValue.IsValid() || !LayerValue->TryGetObject(LayerObj) || !LayerObj ||
                    !(*LayerObj)->TryGetArrayField(TEXT("branchFilters"), Filters) || !Filters)
                {
                    ANIM_ERROR_RESPONSE(TEXT("Each layerSetup entry needs branchFilters: [{ boneName, blendDepth }]"), TEXT("INVALID_LAYER_SETUP"));
                }
                FInputBlendPose& Layer = Layers.AddDefaulted_GetRef();
                for (const TSharedPtr<FJsonValue>& FilterValue : *Filters)
                {
                    const TSharedPtr<FJsonObject>* FilterObj = nullptr;
                    if (!FilterValue.IsValid() || !FilterValue->TryGetObject(FilterObj) || !FilterObj)
                    {
                        ANIM_ERROR_RESPONSE(TEXT("Each branch filter must be an object { boneName, blendDepth }"), TEXT("INVALID_LAYER_SETUP"));
                    }
                    FBranchFilter& Filter = Layer.BranchFilters.AddDefaulted_GetRef();
                    Filter.BoneName = FName(*GetJsonStringField(*FilterObj, TEXT("boneName"), TEXT("")));
                    Filter.BlendDepth = static_cast<int32>(GetJsonNumberField(*FilterObj, TEXT("blendDepth"), 0.0));
                }
            }
        }
        else if (!BoneName.IsEmpty())
        {
            Layers.AddDefaulted_GetRef().BranchFilters.AddDefaulted_GetRef().BoneName = FName(*BoneName);
        }
        const USkeleton* TargetSkeleton = AnimBP->TargetSkeleton;
        for (const FInputBlendPose& Layer : Layers)
        {
            for (const FBranchFilter& Filter : Layer.BranchFilters)
            {
                if (Filter.BoneName.IsNone() || (TargetSkeleton && TargetSkeleton->GetReferenceSkeleton().FindBoneIndex(Filter.BoneName) == INDEX_NONE))
                {
                    ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Bone '%s' is not on the Animation Blueprint's skeleton"), *Filter.BoneName.ToString()), TEXT("BONE_NOT_FOUND"));
                }
            }
        }

        // Create the Layered Bone Blend node
        FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> NodeCreator(*AnimGraph);
        UAnimGraphNode_LayeredBoneBlend* BlendNode = NodeCreator.CreateNode();
        BlendNode->NodePosX = NodePosX;
        BlendNode->NodePosY = NodePosY;
        NodeCreator.Finalize();

        // One blend pose pin per layer; the node is created with one.
        for (int32 PinCount = BlendNode->Node.BlendPoses.Num(); PinCount < Layers.Num(); ++PinCount)
        {
            BlendNode->AddPinToBlendByFilter();
        }
        if (Layers.Num() > 0)
        {
            BlendNode->Node.LayerSetup.SetNum(FMath::Max(BlendNode->Node.LayerSetup.Num(), Layers.Num()));
            for (int32 Index = 0; Index < Layers.Num(); ++Index)
            {
                BlendNode->Node.LayerSetup[Index] = Layers[Index];
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);

        Response->SetNumberField(TEXT("layerCount"), Layers.Num());
        ANIM_SUCCESS_RESPONSE(Layers.Num() > 0
            ? FString::Printf(TEXT("Layered blend per bone node created with %d layer(s)"), Layers.Num())
            : FString(TEXT("Layered blend per bone node created with no branch filters; it blends nothing until layers are set")));
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring

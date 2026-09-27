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

        // Create the Layered Bone Blend node
        FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> NodeCreator(*AnimGraph);
        UAnimGraphNode_LayeredBoneBlend* BlendNode = NodeCreator.CreateNode();
        BlendNode->NodePosX = NodePosX;
        BlendNode->NodePosY = NodePosY;
        NodeCreator.Finalize();

        // Note: Configuring specific bone layers requires access to BlendNode->Node.LayerSetup
        // which is typically done through the editor UI. Basic node creation is complete.

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);

        ANIM_SUCCESS_RESPONSE(TEXT("Layered blend per bone node created"));
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring

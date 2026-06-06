#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleBlueprintTransitionRuleActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("set_transition_rules"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("blueprintPath"), TEXT("")));
        FString StateMachineName = GetStringFieldAnimAuth(Params, TEXT("stateMachineName"), TEXT(""));
        FString FromState = GetStringFieldAnimAuth(Params, TEXT("fromState"), TEXT(""));
        FString ToState = GetStringFieldAnimAuth(Params, TEXT("toState"), TEXT(""));
        float CrossfadeDuration = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("crossfadeDuration"), -1.0));
        int32 PriorityOrder = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("priorityOrder"), -1));
        bool bAutomatic = GetBoolFieldAnimAuth(Params, TEXT("automaticRule"), false);
        bool bBidirectional = GetBoolFieldAnimAuth(Params, TEXT("bidirectional"), false);
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        // Try to find in-memory version first (may have unsaved changes)
        UAnimBlueprint* AnimBP = FindObject<UAnimBlueprint>(nullptr, *BlueprintPath);
        if (!AnimBP)
        {
            // Fall back to loading from disk
            AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *BlueprintPath));
        }
        if (!AnimBP)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Could not load animation blueprint: %s"), *BlueprintPath), TEXT("ANIM_BP_NOT_FOUND"));
        }

#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA && MCP_HAS_ANIM_STATE_TRANSITION
        // Get the main AnimGraph
        UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!AnimGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
        }

        TArray<UAnimGraphNode_StateMachine*> MatchingStateMachines = FindStateMachineNodes(AnimGraph, StateMachineName);
        if (MatchingStateMachines.Num() == 0)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("State machine '%s' not found"), *StateMachineName), TEXT("SM_NOT_FOUND"));
        }

        UAnimStateTransitionNode* TransNode = nullptr;
        for (UAnimGraphNode_StateMachine* MatchingSMNode : MatchingStateMachines)
        {
            if (!MatchingSMNode || !MatchingSMNode->EditorStateMachineGraph)
            {
                continue;
            }

            UAnimationStateMachineGraph* CandidateGraph = Cast<UAnimationStateMachineGraph>(MatchingSMNode->EditorStateMachineGraph);
            if (!CandidateGraph)
            {
                continue;
            }

            TransNode = FindTransitionNode(CandidateGraph, FromState, ToState);
            if (TransNode)
            {
                break;
            }
        }

        if (!TransNode)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Transition from '%s' to '%s' not found"), *FromState, *ToState), TEXT("TRANSITION_NOT_FOUND"));
        }

        // Update transition properties
        if (CrossfadeDuration >= 0.0f)
        {
            TransNode->CrossfadeDuration = CrossfadeDuration;
        }
        if (PriorityOrder >= 0)
        {
            TransNode->PriorityOrder = PriorityOrder;
        }
        TransNode->bAutomaticRuleBasedOnSequencePlayerInState = bAutomatic;
        TransNode->Bidirectional = bBidirectional;

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);

        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Transition rules updated for '%s' -> '%s'"), *FromState, *ToState));
#else
        // AnimGraph headers not available - return error instead of fake success
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot update transition rules for '%s' -> '%s': AnimGraph module headers not available in this build."), *FromState, *ToState),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR

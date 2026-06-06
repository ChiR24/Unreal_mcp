#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleBlueprintStateTransitionActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("add_state"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("blueprintPath"), TEXT("")));
        FString StateMachineName = GetStringFieldAnimAuth(Params, TEXT("stateMachineName"), TEXT(""));
        FString StateName = GetStringFieldAnimAuth(Params, TEXT("stateName"), TEXT(""));
        int32 NodePosX = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionX"), 200));
        int32 NodePosY = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionY"), 0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        if (StateName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("stateName is required"), TEXT("MISSING_STATE_NAME"));
        }

        // Try to find in-memory version first (may have unsaved changes from add_state_machine)
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

#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA
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

        UAnimationStateMachineGraph* SMGraph = nullptr;
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

            if (UAnimStateNode* ExistingState = FindStateNode(CandidateGraph, StateName))
            {
                Response->SetStringField(TEXT("stateName"), ExistingState->GetStateName());
                Response->SetStringField(TEXT("requestedName"), StateName);
                Response->SetStringField(TEXT("stateMachine"), StateMachineName);
                Response->SetBoolField(TEXT("existingAsset"), true);
                ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("State '%s' already exists in state machine '%s'"), *ExistingState->GetStateName(), *StateMachineName));
                return Response;
            }

            if (!SMGraph)
            {
                SMGraph = CandidateGraph;
            }
        }

        if (!SMGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Invalid state machine graph"), TEXT("INVALID_GRAPH"));
        }

        // Create the State Node using FGraphNodeCreator
        FGraphNodeCreator<UAnimStateNode> StateCreator(*SMGraph);
        UAnimStateNode* StateNode = StateCreator.CreateNode();
        StateNode->NodePosX = NodePosX;
        StateNode->NodePosY = NodePosY;
        StateCreator.Finalize();

        // IMPORTANT: FGraphNodeCreator does NOT call PostPlacedNewNode(), which is where
        // the BoundGraph is normally created. We must create it manually here.
        // This mirrors UAnimStateNode::PostPlacedNewNode() logic exactly.
        if (!StateNode->BoundGraph)
        {
            // Create the animation state graph (BoundGraph) with NAME_None first
            // This matches UE's PostPlacedNewNode() behavior
            StateNode->BoundGraph = FBlueprintEditorUtils::CreateNewGraph(
                StateNode,
                NAME_None,
                UAnimationStateGraph::StaticClass(),
                UAnimationStateGraphSchema::StaticClass()
            );

            if (StateNode->BoundGraph)
            {
                // Use RenameGraphWithSuggestion for proper name validation (matches UE behavior)
                TSharedPtr<INameValidatorInterface> NameValidator = FNameValidatorFactory::MakeValidator(StateNode);
                FBlueprintEditorUtils::RenameGraphWithSuggestion(StateNode->BoundGraph, NameValidator, *StateName);

                // Initialize the state graph with default nodes (result node, etc.)
                const UEdGraphSchema* StateSchema = StateNode->BoundGraph->GetSchema();
                if (StateSchema)
                {
                    StateSchema->CreateDefaultNodesForGraph(*StateNode->BoundGraph);
                }

                // Add the new graph as a child of the state machine graph
                if (SMGraph->SubGraphs.Find(StateNode->BoundGraph) == INDEX_NONE)
                {
                    SMGraph->SubGraphs.Add(StateNode->BoundGraph);
                }
            }
        }
        else
        {
            // BoundGraph already exists (shouldn't happen with FGraphNodeCreator), rename it
            FBlueprintEditorUtils::RenameGraph(StateNode->BoundGraph, *StateName);
        }

        // Get the actual state name that was assigned (may differ from requested due to validation)
        FString ActualStateName = StateNode->GetStateName();

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);

        Response->SetStringField(TEXT("stateName"), ActualStateName);
        Response->SetStringField(TEXT("requestedName"), StateName);
        Response->SetStringField(TEXT("stateMachine"), StateMachineName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("State '%s' created in state machine '%s'"), *ActualStateName, *StateMachineName));
#else
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("State '%s' marked for creation (requires AnimGraph module)"), *StateName));
#endif
        return Response;
    }

    if (SubAction == TEXT("add_transition"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("blueprintPath"), TEXT("")));
        FString StateMachineName = GetStringFieldAnimAuth(Params, TEXT("stateMachineName"), TEXT(""));
        FString FromState = GetStringFieldAnimAuth(Params, TEXT("fromState"), TEXT(""));
        FString ToState = GetStringFieldAnimAuth(Params, TEXT("toState"), TEXT(""));
        float CrossfadeDuration = static_cast<float>(GetNumberFieldAnimAuth(Params, TEXT("crossfadeDuration"), 0.2));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        if (FromState.IsEmpty() || ToState.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("fromState and toState are required"), TEXT("MISSING_STATES"));
        }

        // Try to find in-memory version first (may have unsaved changes from add_state)
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

        UAnimationStateMachineGraph* SMGraph = nullptr;
        bool bFoundSourceStateAnywhere = false;
        bool bFoundTargetStateAnywhere = false;

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

            UAnimStateNode* CandidateFrom = FindStateNode(CandidateGraph, FromState);
            UAnimStateNode* CandidateTo = FindStateNode(CandidateGraph, ToState);
            bFoundSourceStateAnywhere = bFoundSourceStateAnywhere || CandidateFrom != nullptr;
            bFoundTargetStateAnywhere = bFoundTargetStateAnywhere || CandidateTo != nullptr;

            if (CandidateFrom && CandidateTo)
            {
                SMGraph = CandidateGraph;

                if (UAnimStateTransitionNode* ExistingTransition = FindTransitionNode(CandidateGraph, FromState, ToState))
                {
                    Response->SetStringField(TEXT("fromState"), FromState);
                    Response->SetStringField(TEXT("toState"), ToState);
                    Response->SetNumberField(TEXT("crossfadeDuration"), ExistingTransition->CrossfadeDuration);
                    Response->SetBoolField(TEXT("existingAsset"), true);
                    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Transition from '%s' to '%s' already exists"), *FromState, *ToState));
                    return Response;
                }

                break;
            }
        }

        if (!bFoundSourceStateAnywhere)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Source state '%s' not found"), *FromState), TEXT("SOURCE_STATE_NOT_FOUND"));
        }
        if (!bFoundTargetStateAnywhere)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Target state '%s' not found"), *ToState), TEXT("TARGET_STATE_NOT_FOUND"));
        }

        if (!SMGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Invalid state machine graph"), TEXT("INVALID_GRAPH"));
        }

        // Find the source and target states in the selected graph
        UAnimStateNode* FromNode = FindStateNode(SMGraph, FromState);
        UAnimStateNode* ToNode = FindStateNode(SMGraph, ToState);

        if (!FromNode)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Source state '%s' not found"), *FromState), TEXT("SOURCE_STATE_NOT_FOUND"));
        }
        if (!ToNode)
        {
            ANIM_ERROR_RESPONSE(FString::Printf(TEXT("Target state '%s' not found"), *ToState), TEXT("TARGET_STATE_NOT_FOUND"));
        }

        // Create the Transition Node
        FGraphNodeCreator<UAnimStateTransitionNode> TransCreator(*SMGraph);
        UAnimStateTransitionNode* TransNode = TransCreator.CreateNode();
        TransCreator.Finalize();

        // Establish the connection between states
        TransNode->CreateConnections(FromNode, ToNode);

        // Configure transition properties
        TransNode->CrossfadeDuration = CrossfadeDuration;
        TransNode->BlendMode = EAlphaBlendOption::Linear;

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);

        Response->SetStringField(TEXT("fromState"), FromState);
        Response->SetStringField(TEXT("toState"), ToState);
        Response->SetNumberField(TEXT("crossfadeDuration"), CrossfadeDuration);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("Transition from '%s' to '%s' created"), *FromState, *ToState));
#else
        // AnimGraph headers not available - return error instead of fake success
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot create transition from '%s' to '%s': AnimGraph module headers not available in this build."), *FromState, *ToState),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR

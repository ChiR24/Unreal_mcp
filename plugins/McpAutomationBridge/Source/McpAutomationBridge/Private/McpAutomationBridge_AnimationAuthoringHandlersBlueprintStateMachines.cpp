#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

TSharedPtr<FJsonObject> HandleBlueprintStateMachineActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction == TEXT("add_state_machine"))
    {
        FString BlueprintPath = NormalizeAnimPath(GetStringFieldAnimAuth(Params, TEXT("blueprintPath"), TEXT("")));
        FString StateMachineName = GetStringFieldAnimAuth(Params, TEXT("stateMachineName"), TEXT(""));
        int32 NodePosX = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionX"), 0));
        int32 NodePosY = static_cast<int32>(GetNumberFieldAnimAuth(Params, TEXT("positionY"), 0));
        bool bSave = GetBoolFieldAnimAuth(Params, TEXT("save"), true);

        if (StateMachineName.IsEmpty())
        {
            ANIM_ERROR_RESPONSE(TEXT("stateMachineName is required"), TEXT("MISSING_STATE_MACHINE_NAME"));
        }

        // Try to find in-memory version first (may have unsaved changes from create_anim_blueprint)
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

        if (UAnimGraphNode_StateMachine* ExistingSMNode = FindStateMachineNode(AnimGraph, StateMachineName))
        {
            Response->SetStringField(TEXT("nodeName"), ExistingSMNode->GetStateMachineName());
            Response->SetBoolField(TEXT("existingAsset"), true);
            ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("State machine '%s' already exists in %s"), *StateMachineName, *BlueprintPath));
            return Response;
        }

        // Create the State Machine Node using FGraphNodeCreator
        FGraphNodeCreator<UAnimGraphNode_StateMachine> NodeCreator(*AnimGraph);
        UAnimGraphNode_StateMachine* SMNode = NodeCreator.CreateNode();
        SMNode->NodePosX = NodePosX;
        SMNode->NodePosY = NodePosY;
        NodeCreator.Finalize();

        // Create the internal State Machine Graph using FBlueprintEditorUtils
        UAnimationStateMachineGraph* InnerGraph = Cast<UAnimationStateMachineGraph>(
            FBlueprintEditorUtils::CreateNewGraph(
                AnimBP,
                FName(*StateMachineName),
                UAnimationStateMachineGraph::StaticClass(),
                UAnimationStateMachineSchema::StaticClass()
            )
        );
        if (!InnerGraph)
        {
            ANIM_ERROR_RESPONSE(TEXT("Failed to create animation state machine graph"), TEXT("CREATE_GRAPH_FAILED"));
        }

        // Link the State Machine Node to its internal graph
        SMNode->EditorStateMachineGraph = InnerGraph;
        InnerGraph->OwnerAnimGraphNode = SMNode;

        // Initialize Entry Node (required for State Machines)
        const UAnimationStateMachineSchema* Schema = Cast<UAnimationStateMachineSchema>(InnerGraph->GetSchema());
        if (!Schema)
        {
            ANIM_ERROR_RESPONSE(TEXT("Animation state machine graph has an invalid schema"), TEXT("INVALID_SCHEMA"));
        }
        Schema->CreateDefaultNodesForGraph(*InnerGraph);

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBP);
        SaveAnimAsset(AnimBP, bSave);

        Response->SetStringField(TEXT("nodeName"), StateMachineName);
        ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("State machine '%s' created with entry node"), *StateMachineName));
#else
        // AnimGraph headers not available - return error instead of fake success
        ANIM_ERROR_RESPONSE(
            FString::Printf(TEXT("Cannot create state machine '%s': AnimGraph module headers not available in this build. Rebuild with AnimGraph module enabled."), *StateMachineName),
            TEXT("ANIMGRAPH_MODULE_UNAVAILABLE"));
#endif
        return Response;
    }
    return nullptr;
}

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR

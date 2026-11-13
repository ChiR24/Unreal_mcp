#include "Domains/AnimationAuthoring/McpAnimSupport.h"

#include "AnimGraphNode_SequencePlayer.h"

// The Anim Blueprint graph nodes more than one action makes: a state machine with its inner graph (add_state_machine,
// and create_state_machine in the animation tool) and a sequence player (set_state_animation, create_blend_tree).
namespace McpAnimationAuthoring
{
UAnimGraphNode_StateMachine* CreateStateMachineNode(UAnimBlueprint* AnimBP, UEdGraph& AnimGraph, const FString& Name,
                                                    int32 X, int32 Y, FString& OutError, FString& OutCode)
{
    FGraphNodeCreator<UAnimGraphNode_StateMachine> NodeCreator(AnimGraph);
    UAnimGraphNode_StateMachine* SMNode = NodeCreator.CreateNode();
    SMNode->NodePosX = X;
    SMNode->NodePosY = Y;
    NodeCreator.Finalize();

    UAnimationStateMachineGraph* InnerGraph = Cast<UAnimationStateMachineGraph>(FBlueprintEditorUtils::CreateNewGraph(
        AnimBP, FName(*Name), UAnimationStateMachineGraph::StaticClass(), UAnimationStateMachineSchema::StaticClass()));
    if (!InnerGraph)
    {
        OutError = TEXT("Failed to create animation state machine graph");
        OutCode = TEXT("CREATE_GRAPH_FAILED");
        return nullptr;
    }
    SMNode->EditorStateMachineGraph = InnerGraph;
    InnerGraph->OwnerAnimGraphNode = SMNode;

    // The entry node every state machine needs.
    const UAnimationStateMachineSchema* Schema = Cast<UAnimationStateMachineSchema>(InnerGraph->GetSchema());
    if (!Schema)
    {
        OutError = TEXT("Animation state machine graph has an invalid schema");
        OutCode = TEXT("INVALID_SCHEMA");
        return nullptr;
    }
    Schema->CreateDefaultNodesForGraph(*InnerGraph);
    return SMNode;
}

UAnimGraphNode_SequencePlayer* CreateSequencePlayer(UEdGraph& Graph, UAnimSequenceBase* Sequence, int32 X, int32 Y)
{
    FGraphNodeCreator<UAnimGraphNode_SequencePlayer> Creator(Graph);
    UAnimGraphNode_SequencePlayer* Player = Creator.CreateNode();
    Player->NodePosX = X;
    Player->NodePosY = Y;
    // Write the reflected property, as the details panel does: Node.Sequence is protected, and 5.0's SetSequence
    // also writes anim-class node data that a graph node, not yet compiled into a class, does not have.
    const FStructProperty* NodeProperty = FindFProperty<FStructProperty>(Player->GetClass(), TEXT("Node"));
    if (FObjectPropertyBase* SequenceProperty = NodeProperty ? FindFProperty<FObjectPropertyBase>(NodeProperty->Struct, TEXT("Sequence")) : nullptr)
    {
        SequenceProperty->SetObjectPropertyValue_InContainer(&Player->Node, Sequence);
    }
    Creator.Finalize();
    return Player;
}
} // namespace McpAnimationAuthoring

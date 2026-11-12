#include "Domains/AnimationAuthoring/McpAnimSupport.h"

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/BridgeHelpers/Security/McpAssetPathCanonical.h"
#include "Safety/McpSafeOperationsAssetSave.h"

namespace McpAnimationAuthoring {

// Empty means REFUSED, not "unchanged": the shared canonicalizer rejects a
// value whose resolved target it cannot vouch for. Replaying its steps here in
// a different order (alias map before separator normalization) is what let
// "\Content\..." mean one thing to the pre-queue gate and another here.
FString NormalizeAnimPath(const FString& Path)
{
    return McpCanonicalizeContentPath(Path, /*bAssumeGameRoot=*/true);
}

namespace
{
// The T at Path, canonicalized first: null for a path the canonicalizer refuses or one that holds no T.
template <typename T>
T* LoadAnimAssetAt(const FString& Path)
{
    const FString NormalizedPath = NormalizeAnimPath(Path);
    return NormalizedPath.IsEmpty() ? nullptr : Cast<T>(StaticLoadObject(T::StaticClass(), nullptr, *NormalizedPath));
}
}

USkeleton* LoadSkeletonFromPathAnim(const FString& SkeletonPath) { return LoadAnimAssetAt<USkeleton>(SkeletonPath); }
USkeletalMesh* LoadSkeletalMeshFromPathAnim(const FString& MeshPath) { return LoadAnimAssetAt<USkeletalMesh>(MeshPath); }
UAnimSequence* LoadAnimSequenceFromPath(const FString& AnimPath) { return LoadAnimAssetAt<UAnimSequence>(AnimPath); }

UAnimSequence* LoadAnimSequenceOrFail(const FString& Path, const TSharedPtr<FJsonObject>& Response)
{
    UAnimSequence* Sequence = LoadAnimSequenceFromPath(Path);
    if (!Sequence)
    {
        SetAnimError(Response, FString::Printf(TEXT("Could not load animation sequence: %s"), *Path), TEXT("SEQUENCE_NOT_FOUND"));
    }
    return Sequence;
}

UAnimBlueprint* LoadAnimBlueprintOrFail(const FString& Path, const TSharedPtr<FJsonObject>& Response, UEdGraph** OutAnimGraph)
{
    UAnimBlueprint* AnimBP = FindObject<UAnimBlueprint>(nullptr, *Path);
    if (!AnimBP)
    {
        AnimBP = Cast<UAnimBlueprint>(StaticLoadObject(UAnimBlueprint::StaticClass(), nullptr, *Path));
    }
    if (!AnimBP)
    {
        SetAnimError(Response, FString::Printf(TEXT("Could not load animation blueprint: %s"), *Path), TEXT("ANIM_BP_NOT_FOUND"));
        return nullptr;
    }
    if (OutAnimGraph)
    {
        *OutAnimGraph = GetAnimGraphFromBlueprint(AnimBP);
        if (!*OutAnimGraph)
        {
            SetAnimError(Response, TEXT("Could not find AnimGraph in blueprint"), TEXT("GRAPH_NOT_FOUND"));
            return nullptr;
        }
    }
    return AnimBP;
}

// Helper to save asset through the repo's loaded-asset safe save path.
// These assets must persist to disk; leaving them registry-only creates large
// in-memory delete sets that later crash post-response cleanup.
bool SaveAnimAsset(UObject* Asset, bool bShouldSave)
{
    if (!Asset)
    {
        return true;
    }
    // save false keeps the edit as an unsaved change: unmarked, it was in no save list and went with the editor.
    Asset->MarkPackageDirty();
    if (!bShouldSave)
    {
        return true;
    }
    McpSafeOperations::McpAnnounceIfNew(Asset);

    const bool bSaved = SaveLoadedAssetThrottled(Asset, true);
    if (bSaved)
    {
        ScanPathSynchronous(Asset->GetOutermost()->GetName());
    }

    return bSaved;
}

// ============================================================================
// AnimGraph Helper Functions for State Machine Implementation
// ============================================================================

// Helper to find the main AnimGraph from an Animation Blueprint
UEdGraph* GetAnimGraphFromBlueprint(UAnimBlueprint* AnimBP)
{
    if (!AnimBP)
    {
        return nullptr;
    }

    // Search through UbergraphPages first (most common location for AnimGraph)
    for (UEdGraph* Graph : AnimBP->UbergraphPages)
    {
        if (Graph && Graph->GetName() == TEXT("AnimGraph"))
        {
            return Graph;
        }
    }

    // Also search through function graphs
    for (UEdGraph* Graph : AnimBP->FunctionGraphs)
    {
        if (Graph && Graph->GetName() == TEXT("AnimGraph"))
        {
            return Graph;
        }
    }

    // Fallback: look for any graph with AnimGraph in the name
    for (UEdGraph* Graph : AnimBP->UbergraphPages)
    {
        if (Graph && Graph->GetName().Contains(TEXT("AnimGraph")))
        {
            return Graph;
        }
    }

    return nullptr;
}

// Helper to find a State Machine node by name in a graph
UAnimGraphNode_StateMachine* FindStateMachineNode(UEdGraph* Graph, const FString& Name)
{
    if (!Graph)
    {
        return nullptr;
    }

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node))
        {
            // Check node title for matching name
            FString NodeName = SMNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
            if (NodeName.Contains(Name) || SMNode->GetStateMachineName() == Name)
            {
                return SMNode;
            }
        }
    }

    return nullptr;
}

// Helper to collect all state machine nodes that match a requested name.
TArray<UAnimGraphNode_StateMachine*> FindStateMachineNodes(UEdGraph* Graph, const FString& Name)
{
    TArray<UAnimGraphNode_StateMachine*> Matches;
    if (!Graph)
    {
        return Matches;
    }

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node))
        {
            const FString MachineName = SMNode->GetStateMachineName();
            const FString Title = SMNode->GetNodeTitle(ENodeTitleType::ListView).ToString();
            const bool bExactNameMatch = MachineName.Equals(Name, ESearchCase::IgnoreCase);
            const bool bTitleMatch = Title.Equals(Name, ESearchCase::IgnoreCase) || Title.Contains(Name);
            if (bExactNameMatch || bTitleMatch)
            {
                Matches.Add(SMNode);
            }
        }
    }

    return Matches;
}

// Helper to find a State node within a State Machine graph
UAnimStateNode* FindStateNode(UAnimationStateMachineGraph* SMGraph, const FString& Name)
{
    if (!SMGraph)
    {
        return nullptr;
    }

    for (UEdGraphNode* Node : SMGraph->Nodes)
    {
        if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
        {
            if (StateNode->GetStateName().Equals(Name, ESearchCase::IgnoreCase))
            {
                return StateNode;
            }
        }
    }

    return nullptr;
}

UAnimStateTransitionNode* FindTransitionNode(UAnimationStateMachineGraph* SMGraph, const FString& FromState, const FString& ToState)
{
    if (!SMGraph)
    {
        return nullptr;
    }

    for (UEdGraphNode* Node : SMGraph->Nodes)
    {
        if (UAnimStateTransitionNode* Trans = Cast<UAnimStateTransitionNode>(Node))
        {
            UAnimStateNodeBase* PrevState = Trans->GetPreviousState();
            UAnimStateNodeBase* NextState = Trans->GetNextState();
            if (PrevState && NextState &&
                PrevState->GetStateName().Equals(FromState, ESearchCase::IgnoreCase) &&
                NextState->GetStateName().Equals(ToState, ESearchCase::IgnoreCase))
            {
                return Trans;
            }
        }
    }

    return nullptr;
}

// Every "not found" here used to stop at the name that failed, which left the
// caller nothing to go on: inspect_graph cannot reach a state machine's child
// graph, so the only way to discover a state name was to guess until one
// stopped erroring. These attach the inventory to the response instead.
//
// A machine is addressable by its name OR its node title, and an unnamed
// machine -- which FindStateMachineNodes explicitly tolerates -- has only the
// latter, so reporting the name alone answered with "". Passed back, that
// empty string then matched EVERY machine through Title.Contains(""). The
// title is also what the editor shows and what a user would type, so fall
// back to it.
void AddStateMachineInventory(UEdGraph* AnimGraph, TSharedPtr<FJsonObject> Response)
{
    if (!AnimGraph || !Response.IsValid()) { return; }
    TArray<TSharedPtr<FJsonValue>> Names;
    for (UEdGraphNode* Node : AnimGraph->Nodes)
    {
        if (UAnimGraphNode_StateMachine* SMNode = Cast<UAnimGraphNode_StateMachine>(Node))
        {
            FString Label = SMNode->GetStateMachineName();
            if (Label.IsEmpty()) { Label = SMNode->GetNodeTitle(ENodeTitleType::ListView).ToString(); }
            Names.Add(MakeShared<FJsonValueString>(Label));
        }
    }
    Response->SetArrayField(TEXT("availableStateMachines"), Names);
}

// Entries name the machine they came from. An omitted machineName matches
// EVERY machine in the graph (Title.Contains("") is true), so a flat list left
// two same-named states indistinguishable and mixed transitions that can never
// coexist -- and the caller picks the next state name out of this array.
void AddStateInventory(UEdGraph* AnimGraph, const FString& MachineName, TSharedPtr<FJsonObject> Response)
{
    if (!AnimGraph || !Response.IsValid()) { return; }
    TArray<TSharedPtr<FJsonValue>> States;
    TArray<TSharedPtr<FJsonValue>> Transitions;
    for (UAnimGraphNode_StateMachine* Machine : FindStateMachineNodes(AnimGraph, MachineName))
    {
        UAnimationStateMachineGraph* Graph = Machine != nullptr ? Cast<UAnimationStateMachineGraph>(Machine->EditorStateMachineGraph) : nullptr;
        if (Graph == nullptr) { continue; }
        FString Label = Machine->GetStateMachineName();
        if (Label.IsEmpty()) { Label = Machine->GetNodeTitle(ENodeTitleType::ListView).ToString(); }
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UAnimStateNode* StateNode = Cast<UAnimStateNode>(Node))
            {
                States.Add(MakeShared<FJsonValueString>(Label + TEXT(".") + StateNode->GetStateName()));
            }
            else if (UAnimStateTransitionNode* Trans = Cast<UAnimStateTransitionNode>(Node))
            {
                UAnimStateNodeBase* Prev = Trans->GetPreviousState();
                if (UAnimStateNodeBase* Next = Prev != nullptr ? Trans->GetNextState() : nullptr)
                {
                    Transitions.Add(MakeShared<FJsonValueString>(Label + TEXT(": ")
                        + Prev->GetStateName() + TEXT(" -> ") + Next->GetStateName()));
                }
            }
        }
    }
    Response->SetArrayField(TEXT("availableStates"), States);
    Response->SetArrayField(TEXT("availableTransitions"), Transitions);
}

} // namespace McpAnimationAuthoring

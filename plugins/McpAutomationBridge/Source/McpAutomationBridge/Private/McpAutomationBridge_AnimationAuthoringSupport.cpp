#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
namespace McpAnimationAuthoring {

FString NormalizeAnimPath(const FString& Path)
{
    FString Normalized = Path;
    Normalized.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
    Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));

    // Remove trailing slashes
    while (Normalized.EndsWith(TEXT("/")))
    {
        Normalized.LeftChopInline(1);
    }

    return Normalized;
}

// Helper to load skeleton from path
USkeleton* LoadSkeletonFromPathAnim(const FString& SkeletonPath)
{
    FString NormalizedPath = NormalizeAnimPath(SkeletonPath);
    return Cast<USkeleton>(StaticLoadObject(USkeleton::StaticClass(), nullptr, *NormalizedPath));
}

// Helper to load skeletal mesh from path
USkeletalMesh* LoadSkeletalMeshFromPathAnim(const FString& MeshPath)
{
    FString NormalizedPath = NormalizeAnimPath(MeshPath);
    return Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, *NormalizedPath));
}

// Helper to load animation sequence from path
UAnimSequence* LoadAnimSequenceFromPath(const FString& AnimPath)
{
    FString NormalizedPath = NormalizeAnimPath(AnimPath);
    return Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, *NormalizedPath));
}

// Helper to save asset through the repo's loaded-asset safe save path.
// These assets must persist to disk; leaving them registry-only creates large
// in-memory delete sets that later crash post-response cleanup.
bool SaveAnimAsset(UObject* Asset, bool bShouldSave)
{
    if (!bShouldSave || !Asset)
    {
        return true;
    }

    Asset->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Asset);

    const bool bSaved = SaveLoadedAssetThrottled(Asset, -1.0, true);
    if (bSaved)
    {
        ScanPathSynchronous(Asset->GetOutermost()->GetName());
    }

    return bSaved;
}

// Helper to get FVector from JSON object
FVector GetVectorFromJsonAnim(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid())
    {
        return FVector::ZeroVector;
    }
    return FVector(
        GetNumberFieldAnimAuth(Obj, TEXT("x"), 0.0),
        GetNumberFieldAnimAuth(Obj, TEXT("y"), 0.0),
        GetNumberFieldAnimAuth(Obj, TEXT("z"), 0.0)
    );
}

// Helper to get FRotator from JSON object
FRotator GetRotatorFromJsonAnim(const TSharedPtr<FJsonObject>& Obj)
{
    if (!Obj.IsValid())
    {
        return FRotator::ZeroRotator;
    }
    // Support both Euler (pitch/yaw/roll) and quaternion (x/y/z/w)
    if (Obj->HasField(TEXT("pitch")) || Obj->HasField(TEXT("yaw")) || Obj->HasField(TEXT("roll")))
    {
        return FRotator(
            GetNumberFieldAnimAuth(Obj, TEXT("pitch"), 0.0),
            GetNumberFieldAnimAuth(Obj, TEXT("yaw"), 0.0),
            GetNumberFieldAnimAuth(Obj, TEXT("roll"), 0.0)
        );
    }
    else if (Obj->HasField(TEXT("w")))
    {
        FQuat Quat(
            GetNumberFieldAnimAuth(Obj, TEXT("x"), 0.0),
            GetNumberFieldAnimAuth(Obj, TEXT("y"), 0.0),
            GetNumberFieldAnimAuth(Obj, TEXT("z"), 0.0),
            GetNumberFieldAnimAuth(Obj, TEXT("w"), 1.0)
        );
        return Quat.Rotator();
    }
    return FRotator::ZeroRotator;
}

// ============================================================================
// AnimGraph Helper Functions for State Machine Implementation
// ============================================================================

#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA

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
            // Use GetStateName for more accurate matching
            // Also try case-insensitive matching for robustness
            FString StateName = StateNode->GetStateName();
            if (StateName == Name)
            {
                return StateNode;
            }
            // Case-insensitive fallback
            if (StateName.Equals(Name, ESearchCase::IgnoreCase))
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
#if MCP_HAS_ANIM_STATE_TRANSITION
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
#endif
    }

    return nullptr;
}

#endif // MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR

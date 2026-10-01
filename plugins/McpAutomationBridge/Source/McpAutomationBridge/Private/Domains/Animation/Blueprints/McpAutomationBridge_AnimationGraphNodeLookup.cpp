#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"

#include "AnimGraphNode_AssetPlayerBase.h"

// What set_anim_graph_node_value needs to find a node and to write one of its settings. Lives beside the other
// Animation/Blueprints graph authors because Domains/AnimationAuthoring sits at the 25-file cap.
namespace McpAnimationAuthoring
{
namespace
{
// The AnimGraph and every graph under it: its state machines and their states, where most Sequence Players live.
TArray<UEdGraph*> AnimGraphsOf(UAnimBlueprint* AnimBP)
{
    TArray<UEdGraph*> Graphs;
    if (UEdGraph* AnimGraph = GetAnimGraphFromBlueprint(AnimBP))
    {
        Graphs.Add(AnimGraph);
        AnimGraph->GetAllChildrenGraphs(Graphs);
    }
    return Graphs;
}
} // namespace

TSharedPtr<FJsonValue> DescribeAnimGraphNode(const UEdGraphNode* Node)
{
    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("name"), Node->GetName());
    Entry->SetStringField(TEXT("nodeId"), Node->NodeGuid.ToString());
    Entry->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
    Entry->SetStringField(TEXT("class"), Node->GetClass()->GetName());
    if (const UEdGraph* Graph = Node->GetGraph())
    {
        Entry->SetStringField(TEXT("graph"), Graph->GetName());
    }
    return MakeShared<FJsonValueObject>(Entry);
}

TArray<TSharedPtr<FJsonValue>> ListAnimGraphNodes(UAnimBlueprint* AnimBP, const int32 Max)
{
    TArray<TSharedPtr<FJsonValue>> Nodes;
    for (UEdGraph* Graph : AnimGraphsOf(AnimBP))
    {
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && Nodes.Num() < Max)
            {
                Nodes.Add(DescribeAnimGraphNode(Node));
            }
        }
    }
    return Nodes;
}

UEdGraphNode* FindAnimGraphNode(UAnimBlueprint* AnimBP, const FString& Requested, TArray<UEdGraphNode*>& OutAmbiguous)
{
    FGuid Guid;
    const bool bIsGuid = FGuid::Parse(Requested, Guid);
    TArray<UEdGraphNode*> ByComment;
    TArray<UEdGraphNode*> ByTitle;
    for (UEdGraph* Graph : AnimGraphsOf(AnimBP))
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node)
            {
                continue;
            }
            // What the caller was handed (the object name, the nodeId) wins outright.
            if (Node->GetName().Equals(Requested, ESearchCase::IgnoreCase) || (bIsGuid && Node->NodeGuid == Guid))
            {
                return Node;
            }
            // A custom name (NodeComment, set by add_blend_node) outranks the title, which many nodes share.
            if (Node->NodeComment.Contains(Requested))
            {
                ByComment.Add(Node);
            }
            else if (Node->GetNodeTitle(ENodeTitleType::ListView).ToString().Contains(Requested))
            {
                ByTitle.Add(Node);
            }
        }
    }
    const TArray<UEdGraphNode*>& Candidates = ByComment.Num() > 0 ? ByComment : ByTitle;
    if (Candidates.Num() == 1)
    {
        return Candidates[0];
    }
    OutAmbiguous = Candidates;
    return nullptr;
}

FProperty* ResolveAnimNodeProperty(UEdGraphNode* Node, const FString& Path, void*& OutContainer, FString& OutMissing)
{
    TArray<FString> Parts;
    Path.ParseIntoArray(Parts, TEXT("."));
    OutContainer = Node;
    const UStruct* Scope = Node->GetClass();
    FProperty* Property = nullptr;
    for (int32 Index = 0; Index < Parts.Num(); ++Index)
    {
        Property = Scope->FindPropertyByName(FName(*Parts[Index]));
        if (!Property && Index == 0)
        {
            // AnimGraph nodes keep their runtime settings (Alpha, BlendTime, Sequence ...) in the embedded `Node` struct.
            if (FStructProperty* NodeStruct = CastField<FStructProperty>(Node->GetClass()->FindPropertyByName(TEXT("Node"))))
            {
                OutContainer = NodeStruct->ContainerPtrToValuePtr<void>(Node);
                Scope = NodeStruct->Struct;
                Property = Scope->FindPropertyByName(FName(*Parts[Index]));
            }
        }
        if (!Property)
        {
            OutMissing = Parts[Index];
            return nullptr;
        }
        if (Index + 1 < Parts.Num())
        {
            FStructProperty* Inner = CastField<FStructProperty>(Property);
            if (!Inner)
            {
                OutMissing = Parts[Index + 1];
                return nullptr;
            }
            OutContainer = Inner->ContainerPtrToValuePtr<void>(OutContainer);
            Scope = Inner->Struct;
        }
    }
    return Property;
}

bool IsAnimPlayerAssetProperty(UEdGraphNode* Node, const FProperty* Property)
{
    const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property);
    return Cast<UAnimGraphNode_AssetPlayerBase>(Node) && ObjectProperty && ObjectProperty->PropertyClass &&
           ObjectProperty->PropertyClass->IsChildOf(UAnimationAsset::StaticClass());
}

bool SetAnimPlayerAsset(UEdGraphNode* Node, const TSharedPtr<FJsonValue>& Value, FString& OutAssetPath, FString& OutError, FString& OutErrorCode)
{
    UAnimGraphNode_AssetPlayerBase* Player = Cast<UAnimGraphNode_AssetPlayerBase>(Node);
    FString Requested;
    if (!Player || !Value.IsValid() || !Value->TryGetString(Requested) || Requested.IsEmpty())
    {
        OutError = TEXT("value must be the animation asset path (a /Game path) for the asset this node plays.");
        OutErrorCode = TEXT("INVALID_VALUE");
        return false;
    }
    const FString Normalized = NormalizeAnimPath(Requested);
    UAnimationAsset* Asset = Normalized.IsEmpty() ? nullptr : Cast<UAnimationAsset>(StaticLoadObject(UAnimationAsset::StaticClass(), nullptr, *Normalized));
    if (!Asset)
    {
        OutError = FString::Printf(TEXT("Could not load animation asset: %s"), *Requested);
        OutErrorCode = TEXT("ASSET_NOT_FOUND");
        return false;
    }
    // The engine's own rule for dropping an asset on a node: a node that plays no such class has no setter for it.
    if (Player->SupportsAssetClass(Asset->GetClass()) == EAnimAssetHandlerType::NotSupported)
    {
        OutError = FString::Printf(TEXT("'%s' does not play a %s."), *Player->GetName(), *Asset->GetClass()->GetName());
        OutErrorCode = TEXT("ASSET_TYPE_MISMATCH");
        return false;
    }
    // The node struct holds the asset as an editor-only, constant-folded property: its own setter writes it, and the
    // node reads it back, so a value that did not take is an error and not a success.
    Player->Modify();
    Player->SetAnimationAsset(Asset);
    if (Player->GetAnimationAsset() != Asset)
    {
        const UAnimationAsset* Now = Player->GetAnimationAsset();
        OutError = FString::Printf(TEXT("The node did not take %s (it plays %s)."), *Asset->GetPathName(), Now ? *Now->GetPathName() : TEXT("nothing"));
        OutErrorCode = TEXT("PROPERTY_SET_FAILED");
        return false;
    }
    OutAssetPath = Asset->GetPathName();
    return true;
}

} // namespace McpAnimationAuthoring

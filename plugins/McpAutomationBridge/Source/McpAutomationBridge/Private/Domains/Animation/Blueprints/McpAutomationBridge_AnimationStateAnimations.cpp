#include "Domains/AnimationAuthoring/McpAutomationBridge_AnimationAuthoringSupport.h"

#if WITH_EDITOR
#if __has_include("AnimGraphNode_SequencePlayer.h") && __has_include("AnimGraphNode_StateResult.h")
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_StateResult.h"
#define MCP_HAS_ANIM_STATE_PLAYER 1
#else
#define MCP_HAS_ANIM_STATE_PLAYER 0
#endif

#if __has_include("AnimStateEntryNode.h")
#include "AnimStateEntryNode.h"
#define MCP_HAS_ANIM_STATE_ENTRY 1
#else
#define MCP_HAS_ANIM_STATE_ENTRY 0
#endif

namespace McpAnimationAuthoring
{
namespace
{
UEdGraphPin *FindStatePin(UEdGraphNode *Node, const TCHAR *Name,
                          EEdGraphPinDirection Direction)
{
    if (!Node)
    {
        return nullptr;
    }
    for (UEdGraphPin *Pin : Node->Pins)
    {
        if (Pin && Pin->Direction == Direction &&
            Pin->PinName.ToString().Equals(Name, ESearchCase::IgnoreCase))
        {
            return Pin;
        }
    }
    return nullptr;
}

UEdGraphPin *SolePin(UEdGraphNode *Node, EEdGraphPinDirection Direction)
{
    if (Node)
    {
        for (UEdGraphPin *Pin : Node->Pins)
        {
            if (Pin && Pin->Direction == Direction)
            {
                return Pin;
            }
        }
    }
    return nullptr;
}
} // namespace

TArray<FString> ReadStateAnimationPaths(const TSharedPtr<FJsonObject> &Params)
{
    TArray<FString> Paths;
    const TArray<TSharedPtr<FJsonValue>> *Values = nullptr;
    if (Params.IsValid() && Params->TryGetArrayField(TEXT("animations"), Values) && Values)
    {
        for (const TSharedPtr<FJsonValue> &Value : *Values)
        {
            if (Value.IsValid() && Value->Type == EJson::String && !Value->AsString().IsEmpty())
            {
                Paths.Add(Value->AsString());
            }
        }
    }
    return Paths;
}

#if MCP_HAS_ANIM_STATE_MACHINE_GRAPH && MCP_HAS_ANIM_STATE_MACHINE_SCHEMA
// add_state used to create an EMPTY state and answer success: the animations
// the caller passed were never read at all. A whole locomotion state machine
// could be authored, compiled and saved without one frame of animation in it --
// the character just stood in its reference pose, every call reported success,
// and nothing in any reply said why.
//
// Only the FIRST animation can drive the state, because a state graph has a
// single Result pose. The rest are still created, and every entry reports its
// own `connected` flag, so a caller can see exactly what plays instead of
// having to guess.
//
// Lives beside the other Animation/Blueprints graph authors rather than next to
// its caller only because Domains/AnimationAuthoring sits at the 25-file cap.
void ApplyStateAnimations(UAnimStateNode *StateNode, const TArray<FString> &AnimPaths,
                          TSharedPtr<FJsonObject> Response)
{
    if (AnimPaths.Num() == 0 || !Response.IsValid())
    {
        return;
    }
    TArray<TSharedPtr<FJsonValue>> Applied;
    TArray<TSharedPtr<FJsonValue>> Failed;
#if MCP_HAS_ANIM_STATE_PLAYER
    UEdGraph *StateGraph = StateNode ? StateNode->BoundGraph : nullptr;
    UAnimGraphNode_StateResult *ResultNode = nullptr;
    if (StateGraph)
    {
        for (UEdGraphNode *Node : StateGraph->Nodes)
        {
            ResultNode = Cast<UAnimGraphNode_StateResult>(Node);
            if (ResultNode)
            {
                break;
            }
        }
    }
    const UEdGraphSchema *Schema = StateGraph ? StateGraph->GetSchema() : nullptr;
    for (int32 Index = 0; Index < AnimPaths.Num(); ++Index)
    {
        UAnimSequenceBase *Sequence =
            StateGraph ? LoadObject<UAnimSequenceBase>(nullptr, *NormalizeAnimPath(AnimPaths[Index]))
                       : nullptr;
        if (!Sequence)
        {
            Failed.Add(MakeShared<FJsonValueString>(AnimPaths[Index]));
            continue;
        }
        FGraphNodeCreator<UAnimGraphNode_SequencePlayer> Creator(*StateGraph);
        UAnimGraphNode_SequencePlayer *Player = Creator.CreateNode();
        Player->NodePosX = -400;
        Player->NodePosY = Index * 160;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
        Player->Node.Sequence = Sequence;
#else
        Player->Node.SetSequence(Sequence);
#endif
        Creator.Finalize();
        bool bConnected = false;
        UEdGraphPin *Out = FindStatePin(Player, TEXT("Pose"), EGPD_Output);
        UEdGraphPin *In = FindStatePin(ResultNode, TEXT("Result"), EGPD_Input);
        // Let the schema break the old pose link, not us. A Result pin takes one
        // link, so TryCreateConnection answers BREAK_OTHERS_B and clears it
        // itself -- and answers DISALLOW without touching anything. Breaking the
        // link first instead meant a refused connection left the state with no
        // pose at all, which is worse than the pose it already had.
        if (Index == 0 && Out && In && Schema)
        {
            bConnected = Schema->TryCreateConnection(Out, In);
        }
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("animationPath"), Sequence->GetPathName());
        Entry->SetStringField(TEXT("nodeId"), Player->NodeGuid.ToString());
        Entry->SetBoolField(TEXT("connected"), bConnected);
        Applied.Add(MakeShared<FJsonValueObject>(Entry));
    }
#else
    for (const FString &Path : AnimPaths)
    {
        Failed.Add(MakeShared<FJsonValueString>(Path));
    }
#endif
    Response->SetArrayField(TEXT("animationsApplied"), Applied);
    if (Failed.Num() > 0)
    {
        Response->SetArrayField(TEXT("animationsFailed"), Failed);
    }
}

// A state machine with no entry connection never runs: it compiles with only
// "There was no entry state connection", and the character holds its reference
// pose no matter how many states and transitions were authored. Nothing in the
// action surface could set the entry, so EVERY machine built through MCP came
// out inert. The first state added now becomes the entry, which is what the
// editor does when you drop a state into an empty machine; an entry that is
// already wired is left alone.
void EnsureStateMachineEntry(UAnimationStateMachineGraph *SMGraph,
                             UAnimStateNode *StateNode, TSharedPtr<FJsonObject> Response)
{
#if MCP_HAS_ANIM_STATE_ENTRY
    if (!SMGraph || !StateNode)
    {
        return;
    }
    for (UEdGraphNode *Node : SMGraph->Nodes)
    {
        UAnimStateEntryNode *Entry = Cast<UAnimStateEntryNode>(Node);
        UEdGraphPin *EntryPin = Entry ? FindStatePin(Entry, TEXT("Entry"), EGPD_Output) : nullptr;
        if (!EntryPin && Entry)
        {
            EntryPin = SolePin(Entry, EGPD_Output);
        }
        if (!EntryPin || EntryPin->LinkedTo.Num() > 0)
        {
            continue;
        }
        UEdGraphPin *StatePin = SolePin(StateNode, EGPD_Input);
        if (StatePin)
        {
            EntryPin->MakeLinkTo(StatePin);
            if (Response.IsValid())
            {
                Response->SetStringField(TEXT("entryState"), StateNode->GetStateName());
            }
        }
        return;
    }
#endif
}
#endif

} // namespace McpAnimationAuthoring
#endif // WITH_EDITOR

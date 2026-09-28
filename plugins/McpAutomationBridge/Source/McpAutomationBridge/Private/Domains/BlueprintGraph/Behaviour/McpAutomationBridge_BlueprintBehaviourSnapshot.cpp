// Snapshot and rollback for McpBlueprintBehaviour::Author. Rollback is snapshot-
// based, not undo-based: member steps compile, and a compile can reset the
// editor's undo buffer (McpCompileBlueprintWithDiagnostics).
#include "Domains/BlueprintGraph/Behaviour/McpAutomationBridge_BlueprintBehaviour.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintDiagnostics.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpBlueprintBehaviour
{
namespace
{
TArray<UEdGraph*> SnapshotMemberGraphs(UBlueprint* Blueprint)
{
    TArray<UEdGraph*> Graphs(Blueprint->FunctionGraphs);
    Graphs.Append(Blueprint->DelegateSignatureGraphs);
    Graphs.Append(Blueprint->UbergraphPages);
    return Graphs;
}

UEdGraphPin* SnapshotPin(const TMap<FGuid, UEdGraphNode*>& Live, const FGuid& NodeGuid, const FGuid& PinId)
{
    UEdGraphNode* const* Node = Live.Find(NodeGuid);
    if (!Node)
    {
        return nullptr;
    }
    for (UEdGraphPin* Pin : (*Node)->Pins)
    {
        if (Pin && Pin->PinId == PinId)
        {
            return Pin;
        }
    }
    return nullptr;
}

void SnapshotNode(UEdGraphNode& Node, FSnapshot& Out)
{
    Out.NodeGuids.Add(Node.NodeGuid);
    FNodeState& State = Out.Nodes.AddDefaulted_GetRef();
    State.NodeGuid = Node.NodeGuid;
    State.Comment = Node.NodeComment;
    State.EnabledState = Node.GetDesiredEnabledState();
    State.bUserSetEnabledState = Node.HasUserSetTheEnabledState();
    State.bCommentBubbleVisible = Node.bCommentBubbleVisible != 0;
    for (UEdGraphPin* Pin : Node.Pins)
    {
        if (!Pin)
        {
            continue;
        }
        FPinState& PinState = Out.Pins.AddDefaulted_GetRef();
        PinState.NodeGuid = Node.NodeGuid;
        PinState.PinId = Pin->PinId;
        PinState.DefaultValue = Pin->DefaultValue;
        PinState.DefaultObject = Pin->DefaultObject.Get();
        PinState.DefaultTextValue = Pin->DefaultTextValue;
        for (UEdGraphPin* Linked : Pin->LinkedTo)
        {
            if (Linked)
            {
                PinState.Links.Emplace(Linked->GetOwningNode()->NodeGuid, Linked->PinId);
            }
        }
    }
}
} // namespace

FSnapshot TakeSnapshot(UBlueprint* Blueprint)
{
    FSnapshot Out;
    if (!Blueprint)
    {
        return Out;
    }
    if (Blueprint->Status == BS_Dirty || Blueprint->Status == BS_Unknown)
    {
        McpSafeCompileBlueprint(Blueprint);
    }
    Out.bCompiled = Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings;
    Out.bPackageDirty = Blueprint->GetOutermost()->IsDirty();
    Out.Variables = Blueprint->NewVariables;
    Out.Graphs.Append(SnapshotMemberGraphs(Blueprint));
    if (Blueprint->SimpleConstructionScript)
    {
        Out.ScsNodes.Append(Blueprint->SimpleConstructionScript->GetAllNodes());
    }
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node)
            {
                SnapshotNode(*Node, Out);
            }
        }
    }
    return Out;
}

namespace Detail
{
void Restore(UBlueprint* Blueprint, const FSnapshot& Before, const TSharedPtr<FJsonObject>& Report)
{
    // By object, not name: a replaced function is a new graph under its old name.
    for (UEdGraph* Graph : SnapshotMemberGraphs(Blueprint))
    {
        if (Graph && !Before.Graphs.Contains(Graph))
        {
            FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph, EGraphRemoveFlags::MarkTransient);
        }
    }
    TMap<FGuid, UEdGraphNode*> Live;
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : TArray<UEdGraphNode*>(Graph->Nodes))
        {
            if (Node && !Before.NodeGuids.Contains(Node->NodeGuid))
            {
                FBlueprintEditorUtils::RemoveNode(Blueprint, Node, /*bDontRecompile=*/true);
            }
            else if (Node)
            {
                Live.Add(Node->NodeGuid, Node);
            }
        }
    }
    // The then_N pin a hook added to an event hub that was already there. Pin ids
    // survive node reconstruction, so an id missing from the snapshot is a new pin.
    TSet<FGuid> PinIds;
    for (const FPinState& State : Before.Pins)
    {
        PinIds.Add(State.PinId);
    }
    for (const TPair<FGuid, UEdGraphNode*>& Pair : Live)
    {
        for (UEdGraphPin* Pin : TArray<UEdGraphPin*>(Pair.Value->Pins))
        {
            if (Pin && Pair.Value->NodeComment == HubTag && !PinIds.Contains(Pin->PinId))
            {
                Pair.Value->RemovePin(Pin);
            }
        }
    }
    // Links first: linking a ghost event makes it a real node and clears its
    // comment, and the node states below put that back.
    for (const FPinState& State : Before.Pins)
    {
        UEdGraphPin* Pin = SnapshotPin(Live, State.NodeGuid, State.PinId);
        if (!Pin)
        {
            continue;
        }
        Pin->BreakAllPinLinks();
        for (const TPair<FGuid, FGuid>& Link : State.Links)
        {
            if (UEdGraphPin* Other = SnapshotPin(Live, Link.Key, Link.Value))
            {
                Pin->MakeLinkTo(Other);
            }
        }
        Pin->DefaultValue = State.DefaultValue;
        Pin->DefaultObject = State.DefaultObject.Get();
        Pin->DefaultTextValue = State.DefaultTextValue;
    }
    for (const FNodeState& State : Before.Nodes)
    {
        if (UEdGraphNode* const* Node = Live.Find(State.NodeGuid))
        {
            (*Node)->NodeComment = State.Comment;
            (*Node)->SetEnabledState(State.EnabledState, State.bUserSetEnabledState);
            (*Node)->bCommentBubbleVisible = State.bCommentBubbleVisible;
        }
    }
    for (int32 Index = Blueprint->NewVariables.Num() - 1; Index >= 0; --Index)
    {
        const FGuid VarGuid = Blueprint->NewVariables[Index].VarGuid;
        const FBPVariableDescription* Old =
            Before.Variables.FindByPredicate([&VarGuid](const FBPVariableDescription& Var) { return Var.VarGuid == VarGuid; });
        if (Old)
        {
            Blueprint->NewVariables[Index] = *Old;
        }
        else
        {
            FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, Blueprint->NewVariables[Index].VarName);
        }
    }
    if (USimpleConstructionScript* Scs = Blueprint->SimpleConstructionScript)
    {
        const TArray<USCS_Node*> Nodes = Scs->GetAllNodes();
        for (int32 Index = Nodes.Num() - 1; Index >= 0; --Index)
        {
            if (Nodes[Index] && !Before.ScsNodes.Contains(Nodes[Index]))
            {
                Scs->RemoveNodeAndPromoteChildren(Nodes[Index]);
            }
        }
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> After = MakeShared<FJsonObject>();
    FString FirstError;
    McpCompileBlueprintWithDiagnostics(Blueprint, After, FirstError, 6);
    Report->SetObjectField(TEXT("afterRollback"), After);
    Report->SetBoolField(TEXT("rolledBack"), true);
}
} // namespace Detail
} // namespace McpBlueprintBehaviour

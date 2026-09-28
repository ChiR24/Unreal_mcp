// Ownership: every node a behaviour makes carries NodeComment "MCP behaviour:
// <Tag> #<NodeGuid>". The guid is the node's own, and a paste gives a copy a new
// one, so a copied node never matches and a replace never deletes the user's copy.
// Shared nodes (hubs, events a hook created, shared custom events, the input
// registration) carry HubTag or InputContextTag instead and are never removed.
#include "Domains/BlueprintGraph/Behaviour/McpAutomationBridge_BlueprintBehaviour.h"

#include "EdGraph/EdGraph.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpBlueprintBehaviour
{
namespace
{
const TCHAR* const OwnerTagPrefix = TEXT("MCP behaviour: ");
}

FString OwnerTag(const FString& Tag, const UEdGraphNode& Node)
{
    return FString::Printf(TEXT("%s%s #%s"), OwnerTagPrefix, *Tag, *Node.NodeGuid.ToString());
}

TArray<UEdGraphNode*> FindOwned(UBlueprint* Blueprint, const FString& Tag)
{
    TArray<UEdGraphNode*> Owned;
    TArray<UEdGraph*> Graphs;
    if (Blueprint)
    {
        Blueprint->GetAllGraphs(Graphs);
    }
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && Detail::IsOwnedBy(*Node, Tag))
            {
                Owned.Add(Node);
            }
        }
    }
    return Owned;
}

namespace Detail
{
bool IsOwnedBy(const UEdGraphNode& Node, const FString& Tag)
{
    return Node.NodeComment == OwnerTag(Tag, Node);
}

bool IsBehaviourOwned(const UEdGraphNode& Node)
{
    return Node.NodeComment.StartsWith(OwnerTagPrefix) &&
           Node.NodeComment.EndsWith(FString::Printf(TEXT(" #%s"), *Node.NodeGuid.ToString()));
}

bool IsFunctionOwnedBy(const UEdGraph* Graph, const FString& Tag)
{
    TArray<UK2Node_FunctionEntry*> Entries;
    if (Graph)
    {
        Graph->GetNodesOfClass(Entries);
    }
    return Entries.Num() > 0 && IsOwnedBy(*Entries[0], Tag);
}

FString CheckRemovable(UBlueprint* Blueprint, const FString& Tag, const TSet<FName>& Rebuilt)
{
    // A replace removes the behaviour's functions and custom events; one that the
    // recipe does not declare again must not be called from a node it does not own.
    TSet<FName> Going;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (IsFunctionOwnedBy(Graph, Tag) && !Rebuilt.Contains(Graph->GetFName()))
        {
            Going.Add(Graph->GetFName());
        }
    }
    TArray<UK2Node_CustomEvent*> Events;
    FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, Events);
    for (UK2Node_CustomEvent* Event : Events)
    {
        if (IsOwnedBy(*Event, Tag) && !Rebuilt.Contains(Event->CustomFunctionName))
        {
            Going.Add(Event->CustomFunctionName);
        }
    }
    TArray<UK2Node_CallFunction*> Calls;
    FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, Calls);
    for (UK2Node_CallFunction* Call : Calls)
    {
        const FName Called = Call->FunctionReference.GetMemberName();
        if (Going.Contains(Called) && !IsOwnedBy(*Call, Tag))
        {
            return FString::Printf(
                TEXT("Behaviour '%s' would remove %s, but graph %s still calls it from a node this behaviour does not "
                     "own. Delete that call first, or keep %s in the recipe."),
                *Tag, *Called.ToString(), *Call->GetGraph()->GetName(), *Called.ToString());
        }
    }
    return FString();
}

int32 RemoveOwned(UBlueprint* Blueprint, const FString& Tag)
{
    int32 Removed = 0;
    for (UEdGraph* Graph : TArray<UEdGraph*>(Blueprint->FunctionGraphs))
    {
        if (IsFunctionOwnedBy(Graph, Tag))
        {
            FBlueprintEditorUtils::RemoveGraph(Blueprint, Graph, EGraphRemoveFlags::MarkTransient);
            ++Removed;
        }
    }
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : TArray<UEdGraphNode*>(Graph->Nodes))
        {
            // An entry node goes only with its whole graph (above); alone it would
            // leave a function the compiler cannot name.
            if (Node && IsOwnedBy(*Node, Tag) && !Node->IsA<UK2Node_FunctionEntry>())
            {
                FBlueprintEditorUtils::RemoveNode(Blueprint, Node, /*bDontRecompile=*/true);
                ++Removed;
            }
        }
    }
    if (Removed > 0)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    }
    return Removed;
}

void TagNew(UBlueprint* Blueprint, const FSnapshot& Before, const FString& Tag, const TMap<FGuid, FString>& SharedTags)
{
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (Node && !Before.NodeGuids.Contains(Node->NodeGuid))
            {
                const FString* Shared = SharedTags.Find(Node->NodeGuid);
                Node->NodeComment = Shared ? *Shared : OwnerTag(Tag, *Node);
            }
        }
    }
}
} // namespace Detail
} // namespace McpBlueprintBehaviour

#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"

#include "Domains/BlueprintGraph/Behaviour/McpBPBehaviourNodes.h"
#include "Editor.h"
#include "K2Node_MacroInstance.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"

namespace McpBlueprintGraphHandlers
{
// ForLoop / WhileLoop / ForEachLoop, DoOnce, Do N, Gate, FlipFlop and the exec IsValid
// are NOT UK2Node_* classes — they are Blueprint macros in the engine StandardMacros
// library, instantiated via K2Node_MacroInstance. Aliases that pointed at a nonexistent
// class (K2Node_ForLoop, K2Node_FlipFlop, K2Node_DoOnce -> NODE_TYPE_NOT_FOUND) or at
// the wrong class (ForEachLoop -> K2Node_ForEachElementInEnum) are gone; create_node and
// add_node both resolve them through this table. FString keys match case-insensitively.
static const TMap<FString, FString>& StandardMacroByType()
{
    static const TMap<FString, FString> Table = {
        {TEXT("ForLoop"), TEXT("ForLoop")},
        {TEXT("ForLoopWithBreak"), TEXT("ForLoopWithBreak")},
        {TEXT("WhileLoop"), TEXT("WhileLoop")},
        {TEXT("ForEachLoop"), TEXT("ForEachLoop")},
        {TEXT("ForEachLoopWithBreak"), TEXT("ForEachLoopWithBreak")},
        {TEXT("DoOnce"), TEXT("DoOnce")},
        {TEXT("DoN"), TEXT("Do N")},
        {TEXT("Gate"), TEXT("Gate")},
        {TEXT("FlipFlop"), TEXT("FlipFlop")},
        {TEXT("IsValid"), TEXT("IsValid")}};
    return Table;
}

const FString* StandardMacroGraphName(const FString& NodeType)
{
    // Accept a bare name or a K2Node_-prefixed alias (callers send both forms).
    const FString* Name = StandardMacroByType().Find(NodeType);
    return Name || !NodeType.StartsWith(TEXT("K2Node_"))
        ? Name
        : StandardMacroByType().Find(NodeType.RightChop(7));
}

static bool TryCreateMacroNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    const FString* MacroGraphName = StandardMacroGraphName(NodeType);
    if (!MacroGraphName)
    {
        // "MacroInstance" is the NODE CLASS, not a macro. Left to the generic
        // path it spawns a UK2Node_MacroInstance with no macro graph attached -
        // a node that reports created and then fails the blueprint with "Macro
        // instance is pointing at an invalid macro graph". Name the spellings
        // that actually resolve instead of building the broken node.
        if (NodeType.Equals(TEXT("MacroInstance"), ESearchCase::IgnoreCase) ||
            NodeType.Equals(TEXT("K2Node_MacroInstance"), ESearchCase::IgnoreCase))
        {
            TArray<FString> Supported;
            StandardMacroByType().GenerateKeyArray(Supported);
            Supported.Sort();
            Context.SendError(
                FString::Printf(
                    TEXT("'MacroInstance' is the node class, not a macro, and "
                         "carries no macro graph on its own. Pass the macro "
                         "itself as nodeType: %s."),
                    *FString::Join(Supported, TEXT(", "))),
                TEXT("MACRO_NAME_REQUIRED"));
            return true;
        }
        return false;
    }

    UBlueprint* MacroLibrary = LoadObject<UBlueprint>(
        nullptr,
        TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
    if (!MacroLibrary)
    {
        Context.SendError(
            TEXT("Could not load engine StandardMacros library."),
            TEXT("MACRO_LIBRARY_NOT_FOUND"));
        return true;
    }

    UEdGraph* MacroGraph = nullptr;
    for (UEdGraph* Graph : MacroLibrary->MacroGraphs)
    {
        if (Graph &&
            Graph->GetName().Equals(*MacroGraphName, ESearchCase::IgnoreCase))
        {
            MacroGraph = Graph;
            break;
        }
    }
    if (!MacroGraph)
    {
        Context.SendError(
            FString::Printf(
                TEXT("Macro '%s' not found in StandardMacros library."),
                **MacroGraphName),
            TEXT("MACRO_NOT_FOUND"));
        return true;
    }

    FGraphNodeCreator<UK2Node_MacroInstance> NodeCreator(*Context.TargetGraph);
    UK2Node_MacroInstance* Node = NodeCreator.CreateNode(false);
    Node->SetMacroGraph(MacroGraph);
    Context.FinalizeNode(NodeCreator, Node, X, Y);
    return true;
}

bool HandleNodeCreationAction(FActionContext& Context)
{
    if (Context.SubAction != TEXT("create_node"))
    {
        return false;
    }

    // A refused node (an unknown function, an overlap, a missing enum) left the Blueprint marked changed and an empty
    // undo step, which the next undo spent instead of the edit before it. A batch keeps its own rollback.
    const bool bOutermost = !(GEditor && GEditor->IsTransactionActive());
    FScopedTransaction Transaction(
        FText::FromString(TEXT("Create Blueprint Node")));
    UPackage* Package = Context.Blueprint->GetOutermost();
    const bool bWasDirty = Package->IsDirty();
    TSet<FGuid> NodesBefore;
    for (const UEdGraphNode* Node : Context.TargetGraph->Nodes)
    {
        NodesBefore.Add(Node ? Node->NodeGuid : FGuid());
    }
    Context.Blueprint->Modify();
    Context.TargetGraph->Modify();

    FString NodeType;
    Context.Payload->TryGetStringField(TEXT("nodeType"), NodeType);
    float X = 0.0f;
    float Y = 0.0f;
    // posX/posY are the declared names, so they win over the legacy x/y (a
    // node read only from x/y landed every declared placement at (0,0)).
    if (!Context.Payload->TryGetNumberField(TEXT("posX"), X))
    {
        Context.Payload->TryGetNumberField(TEXT("x"), X);
    }
    if (!Context.Payload->TryGetNumberField(TEXT("posY"), Y))
    {
        Context.Payload->TryGetNumberField(TEXT("y"), Y);
    }

    if (!(TryCreateCommonFunctionNode(Context, NodeType, X, Y) ||
          TryCreateVariableNode(Context, NodeType, X, Y) ||
          TryCreateFunctionOrEventNode(Context, NodeType, X, Y) ||
          TryCreateCustomEventNode(Context, NodeType, X, Y) ||
          TryCreateMacroNode(Context, NodeType, X, Y) ||
          TryCreateSpecialNode(Context, NodeType, X, Y) ||
          TryCreateBehaviourNode(Context, NodeType, X, Y)))
    {
        CreateDynamicNode(Context, NodeType, X, Y);
    }
    // A node that replaced another of the same name (a custom event) is new too: compare ids, not the count.
    const bool bUnchanged = Context.TargetGraph->Nodes.Num() == NodesBefore.Num() &&
        !Context.TargetGraph->Nodes.ContainsByPredicate(
            [&NodesBefore](const UEdGraphNode* Node) { return Node && !NodesBefore.Contains(Node->NodeGuid); });
    if (bOutermost && bUnchanged)
    {
        Transaction.Cancel();
        if (!bWasDirty)
        {
            Package->SetDirtyFlag(false);
        }
    }
    return true;
}
}

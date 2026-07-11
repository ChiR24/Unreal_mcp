#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "K2Node_MacroInstance.h"
#include "ScopedTransaction.h"

namespace McpBlueprintGraphHandlers
{
// ForLoop / WhileLoop / ForEachLoop and friends are NOT UK2Node_* classes — they
// are Blueprint macros in the engine StandardMacros library, instantiated via
// K2Node_MacroInstance. The old name aliases pointed either at a nonexistent class
// (K2Node_ForLoop/K2Node_WhileLoop -> NODE_TYPE_NOT_FOUND) or at the wrong
// class (ForEachLoop -> K2Node_ForEachElementInEnum, the enum iterator).
// Resolve them to the real macro graph and spawn a macro instance.
static bool TryCreateMacroNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    static const TMap<FString, FString> StandardMacroByType = {
        {TEXT("ForLoop"), TEXT("ForLoop")},
        {TEXT("ForLoopWithBreak"), TEXT("ForLoopWithBreak")},
        {TEXT("WhileLoop"), TEXT("WhileLoop")},
        {TEXT("ForEachLoop"), TEXT("ForEachLoop")},
        {TEXT("ForEachLoopWithBreak"), TEXT("ForEachLoopWithBreak")}};

    // Accept a bare name or a K2Node_-prefixed alias (callers send both forms).
    FString Key = NodeType;
    if (!StandardMacroByType.Contains(Key) && Key.StartsWith(TEXT("K2Node_")))
    {
        Key = Key.RightChop(7);
    }
    const FString* MacroGraphName = StandardMacroByType.Find(Key);
    if (!MacroGraphName)
    {
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

    const FScopedTransaction Transaction(
        FText::FromString(TEXT("Create Blueprint Node")));
    Context.Blueprint->Modify();
    Context.TargetGraph->Modify();

    FString NodeType;
    Context.Payload->TryGetStringField(TEXT("nodeType"), NodeType);
    float X = 0.0f;
    float Y = 0.0f;
    Context.Payload->TryGetNumberField(TEXT("x"), X);
    Context.Payload->TryGetNumberField(TEXT("y"), Y);

    if (TryCreateCommonFunctionNode(Context, NodeType, X, Y) ||
        TryCreateVariableNode(Context, NodeType, X, Y) ||
        TryCreateFunctionOrEventNode(Context, NodeType, X, Y) ||
        TryCreateCustomEventNode(Context, NodeType, X, Y) ||
        TryCreateMacroNode(Context, NodeType, X, Y) ||
        TryCreateSpecialNode(Context, NodeType, X, Y))
    {
        return true;
    }

    CreateDynamicNode(Context, NodeType, X, Y);
    return true;
}
}
#else
namespace McpBlueprintGraphHandlers
{
bool HandleNodeCreationAction(FActionContext&)
{
    return false;
}
}
#endif

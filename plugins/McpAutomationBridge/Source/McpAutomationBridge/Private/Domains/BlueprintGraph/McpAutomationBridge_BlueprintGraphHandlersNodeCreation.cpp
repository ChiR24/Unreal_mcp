#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "K2Node_FunctionEntry.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_DynamicCast.h"
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

void CreateDynamicNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y)
{
    UClass* NodeClass = FindNodeClassByName(NodeType);
    if (!NodeClass)
    {
        Context.SendError(
            FString::Printf(
                TEXT("Node type '%s' not found. Use list_node_types to see available types."),
                *NodeType),
            TEXT("NODE_TYPE_NOT_FOUND"));
        return;
    }

    // Function entry nodes cannot be created standalone: a generically spawned
    // entry has a NAME_None signature, and the next blueprint compile crashes
    // the editor on an engine check() while conforming/renaming that function
    // (ReplaceFunctionReferences). Entries are created as part of add_function.
    if (NodeClass->IsChildOf(UK2Node_FunctionEntry::StaticClass()))
    {
        Context.SendError(
            TEXT("K2Node_FunctionEntry cannot be spawned directly — function entry "
                 "nodes are created (and named) by add_function. Spawning one here "
                 "would leave an unnamed function graph that crashes the editor on "
                 "the next compile."),
            TEXT("NODE_TYPE_NOT_SUPPORTED"));
        return;
    }

    if (TryCreateEnhancedInputNode(Context, NodeClass, X, Y))
    {
        return;
    }

    // DynamicCast nodes must have TargetType set, or they render as an
    // unusable "Bad cast node" (wildcard Object pin, no typed "As <Class>"
    // output). Read targetClass (with legacy fallbacks) and resolve it.
    if (NodeClass->IsChildOf(UK2Node_DynamicCast::StaticClass()))
    {
        FString TargetClass;
        Context.Payload->TryGetStringField(TEXT("targetClass"), TargetClass);
        if (TargetClass.IsEmpty())
        {
            Context.Payload->TryGetStringField(TEXT("memberClass"), TargetClass);
        }
        if (TargetClass.IsEmpty())
        {
            Context.Payload->TryGetStringField(TEXT("nodeClass"), TargetClass);
        }
        if (TargetClass.IsEmpty() &&
            NodeType.StartsWith(TEXT("CastTo"), ESearchCase::IgnoreCase))
        {
            TargetClass = NodeType.Mid(6);
        }
        if (TargetClass.IsEmpty())
        {
            Context.SendError(
                TEXT("DynamicCast node requires a 'targetClass' (Blueprint asset "
                     "path like /Game/Blueprints/BP_Cole, or a class name)."),
                TEXT("INVALID_ARGUMENT"));
            return;
        }

        // Resolve to a UClass. Accept a generated-class path, a Blueprint asset
        // path (append _C), or a bare/native class name.
        UClass* ResolvedTarget = nullptr;
        if (TargetClass.StartsWith(TEXT("/")))
        {
            FString ClassPath = TargetClass;
            if (!ClassPath.EndsWith(TEXT("_C")))
            {
                FString PackageName = ClassPath;
                FString ObjectName = ClassPath;
                int32 DotIdx;
                if (ClassPath.FindChar('.', DotIdx))
                {
                    ObjectName = ClassPath.RightChop(DotIdx + 1);
                }
                else
                {
                    int32 SlashIdx;
                    if (ClassPath.FindLastChar('/', SlashIdx))
                    {
                        ObjectName = ClassPath.RightChop(SlashIdx + 1);
                    }
                }
                ClassPath = PackageName + TEXT(".") + ObjectName + TEXT("_C");
            }
            ResolvedTarget = LoadObject<UClass>(nullptr, *ClassPath);
        }
        if (!ResolvedTarget)
        {
            ResolvedTarget = FindNodeClassByName(TargetClass);
        }
        if (!ResolvedTarget)
        {
            ResolvedTarget = UClass::TryFindTypeSlow<UClass>(TargetClass);
        }
        if (!ResolvedTarget)
        {
            Context.SendError(
                FString::Printf(
                    TEXT("Could not resolve targetClass '%s' for DynamicCast."),
                    *TargetClass),
                TEXT("CLASS_NOT_FOUND"));
            return;
        }

        FGraphNodeCreator<UK2Node_DynamicCast> CastCreator(*Context.TargetGraph);
        UK2Node_DynamicCast* CastNode = CastCreator.CreateNode(false);
        CastNode->TargetType = ResolvedTarget;
        CastNode->SetPurity(false);
        Context.FinalizeNode(CastCreator, CastNode, X, Y);
        return;
    }

    UEdGraphNode* NewNode =
        NewObject<UEdGraphNode>(Context.TargetGraph, NodeClass);
    if (!NewNode)
    {
        Context.SendError(
            TEXT("Failed to instantiate node."),
            TEXT("CREATE_FAILED"));
        return;
    }

    Context.TargetGraph->AddNode(NewNode, false, false);
    NewNode->CreateNewGuid();
    NewNode->PostPlacedNewNode();
    NewNode->AllocateDefaultPins();
    NewNode->NodePosX = X;
    NewNode->NodePosY = Y;
    FBlueprintEditorUtils::MarkBlueprintAsModified(Context.Blueprint);
    SaveLoadedAssetThrottled(Context.Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
    Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
    Result->SetStringField(TEXT("nodeClass"), NodeClass->GetName());
    Context.SendResponse(TEXT("Node created."), Result);
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

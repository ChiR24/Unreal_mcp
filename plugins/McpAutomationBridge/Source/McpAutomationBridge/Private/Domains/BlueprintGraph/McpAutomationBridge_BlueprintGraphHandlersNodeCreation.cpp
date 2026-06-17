#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "K2Node_FunctionEntry.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_CreateWidget.h"
#include "ScopedTransaction.h"

namespace McpBlueprintGraphHandlers
{
// Shared helper: resolve a class string (Blueprint asset path, generated-class
// path, or native class name) to a UClass. Used by every node branch that has
// a class pin (DynamicCast TargetType, CreateWidget WidgetType, etc.) so all
// callers accept the same input formats consistently.
static UClass* ResolveTargetClassFromString(const FString& InClassString)
{
    if (InClassString.IsEmpty())
    {
        return nullptr;
    }

    UClass* Resolved = nullptr;
    if (InClassString.StartsWith(TEXT("/")))
    {
        FString ClassPath = InClassString;
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
        Resolved = LoadObject<UClass>(nullptr, *ClassPath);
    }
    if (!Resolved)
    {
        Resolved = FindNodeClassByName(InClassString);
    }
    if (!Resolved)
    {
        Resolved = UClass::TryFindTypeSlow<UClass>(InClassString);
    }
    return Resolved;
}

// Read the requested target class for a node with a class pin. Checks
// targetClass first, then the legacy memberClass / nodeClass / widgetType
// fields, then peels a "CastTo<Class>" prefix from the nodeType as a final
// fallback. Returns empty string if nothing was supplied.
static FString ReadTargetClassPayload(
    const FActionContext& Context,
    const FString& NodeType)
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
    if (TargetClass.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("widgetType"), TargetClass);
    }
    if (TargetClass.IsEmpty() &&
        NodeType.StartsWith(TEXT("CastTo"), ESearchCase::IgnoreCase))
    {
        TargetClass = NodeType.Mid(6);
    }
    return TargetClass;
}

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
    // output). Read the requested class (with legacy fallbacks) and resolve it.
    if (NodeClass->IsChildOf(UK2Node_DynamicCast::StaticClass()))
    {
        const FString TargetClass = ReadTargetClassPayload(Context, NodeType);
        if (TargetClass.IsEmpty())
        {
            Context.SendError(
                TEXT("DynamicCast node requires a 'targetClass' (Blueprint asset "
                     "path like /Game/Blueprints/BP_Cole, or a class name)."),
                TEXT("INVALID_ARGUMENT"));
            return;
        }
        UClass* ResolvedTarget = ResolveTargetClassFromString(TargetClass);
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

    // CreateWidget nodes carry the widget class as a property on the node
    // (UK2Node_CreateWidget::WidgetType). Without it the node spawns with a
    // generic UUserWidget Class pin and Return Value, so callers can't wire
    // it to anything specific (e.g. an Add to Viewport on the typed widget,
    // or its bindings). Resolve the requested class and assign it, then let
    // ReconstructNode rebuild pins with the correct typed Return Value.
    if (NodeClass->IsChildOf(UK2Node_CreateWidget::StaticClass()))
    {
        const FString TargetClass = ReadTargetClassPayload(Context, NodeType);
        if (TargetClass.IsEmpty())
        {
            Context.SendError(
                TEXT("CreateWidget node requires a 'targetClass' (Widget Blueprint "
                     "asset path like /Game/Widgets/WBP_HUD, or a class name)."),
                TEXT("INVALID_ARGUMENT"));
            return;
        }
        UClass* ResolvedWidget = ResolveTargetClassFromString(TargetClass);
        if (!ResolvedWidget)
        {
            Context.SendError(
                FString::Printf(
                    TEXT("Could not resolve targetClass '%s' for CreateWidget."),
                    *TargetClass),
                TEXT("CLASS_NOT_FOUND"));
            return;
        }

        FGraphNodeCreator<UK2Node_CreateWidget> WidgetCreator(*Context.TargetGraph);
        UK2Node_CreateWidget* WidgetNode = WidgetCreator.CreateNode(false);
        // Bind the widget class on the underlying property so the node knows
        // its concrete type before pin allocation.
        if (FProperty* ClassProp = WidgetNode->GetClass()->FindPropertyByName(
                TEXT("WidgetType")))
        {
            if (FClassProperty* TypedProp = CastField<FClassProperty>(ClassProp))
            {
                TypedProp->SetObjectPropertyValue_InContainer(
                    WidgetNode, ResolvedWidget);
            }
        }
        Context.FinalizeNode(WidgetCreator, WidgetNode, X, Y);
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
    // Some K2 nodes (e.g. UK2Node_FunctionResult) already allocate their default
    // pins inside PostPlacedNewNode(); calling AllocateDefaultPins() again then
    // duplicates them — a FunctionResult ends up with two 'execute' input pins,
    // one of which stays unconnected and trips a compiler warning. Mirror the
    // engine's own FGraphNodeCreator::Finalize guard and only allocate when the
    // node has no pins yet.
    if (NewNode->Pins.Num() == 0)
    {
        NewNode->AllocateDefaultPins();
    }
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

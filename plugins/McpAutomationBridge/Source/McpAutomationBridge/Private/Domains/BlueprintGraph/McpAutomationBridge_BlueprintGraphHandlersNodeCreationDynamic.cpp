#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#include "AnimGraphNode_Base.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_StructOperation.h"

namespace McpBlueprintGraphHandlers
{
// Shared helper: resolve a class string (Blueprint asset path, generated-class
// path, or native class name) to a UClass. Used by every node branch that has
// a class pin (DynamicCast TargetType, CreateWidget WidgetType, etc.) so all
// callers accept the same input formats consistently. Declared in the shared
// private header so other translation units (e.g. the ConstructObject-family
// path in SpecialNodes) resolve classes identically.
UClass* ResolveTargetClassFromString(const FString& InClassString)
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
                // Both parts must be peeled from the same string — keeping
                // PackageName as the full path here produced doubled-up paths
                // like "/Game/X/Y.Y.Y_C" when callers passed an already-fully-
                // qualified object path.
                PackageName = ClassPath.Left(DotIdx);
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
        Resolved = LoadObject<UClass>(nullptr, *ClassPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
    }
    if (!Resolved)
    {
        Resolved = FindNodeClassByName(InClassString);
    }
    if (!Resolved)
    {
        Resolved = McpFindTypeQuiet(InClassString);
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

// The editor offers a node only in a graph whose schema can hold it: an animation node placed in an event graph
// compiled clean and never ran.
FString DescribeGraphMismatch(UClass* NodeClass, const UEdGraph* Graph)
{
    const UEdGraphSchema* Schema = Graph ? Graph->GetSchema() : nullptr;
    if (!NodeClass || !Schema || NodeClass->GetDefaultObject<UEdGraphNode>()->CanCreateUnderSpecifiedSchema(Schema))
    {
        return FString();
    }
    return FString::Printf(TEXT("%s cannot go in graph '%s' (%s)%s"), *NodeClass->GetName(), *Graph->GetName(),
        *Schema->GetClass()->GetName(), NodeClass->IsChildOf(UAnimGraphNode_Base::StaticClass())
            ? TEXT(": an animation node goes in an Animation Blueprint's AnimGraph or a state's graph (graphName names it).")
            : TEXT("."));
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
    const FString Mismatch = DescribeGraphMismatch(NodeClass, Context.TargetGraph);
    if (!Mismatch.IsEmpty())
    {
        Context.SendError(Mismatch, TEXT("NODE_NOT_ALLOWED_IN_GRAPH"));
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

    // Array-function nodes resolve their pins THROUGH a bound array function:
    // GetArrayPins() ensures on TargetFunction and AllocateDefaultPins() ensures on
    // TargetArrayPin. Spawned generically by class name there is no function to bind,
    // so both ensures fire inside the engine before this returns, and the half-built
    // node is left in the graph. Refuse with a usable message instead.
    if (NodeClass->IsChildOf(UK2Node_CallArrayFunction::StaticClass()))
    {
        Context.SendError(
            FString::Printf(
                TEXT("'%s' is an array-function node: its pins are derived from a bound "
                     "array function, so it cannot be spawned by class name (doing so trips "
                     "an engine ensure). Create it as a function call instead — pass the array "
                     "function you want (e.g. Array_Get, Array_Add, Array_Length) as the node "
                     "type, or use list_node_types to find it."),
                *NodeType),
            TEXT("NODE_TYPE_NOT_SUPPORTED"));
        return;
    }

    if (TryCreateEnhancedInputNode(Context, NodeClass, X, Y))
    {
        return;
    }

    // Subsystem getters keep their type in a UPROPERTY rather than a pin, so
    // they must be seeded at construction; see TryCreateSubsystemNode.
    if (TryCreateSubsystemNode(Context, NodeClass, X, Y))
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
        // Cast purity is configurable via the payload; defaults to impure
        // (the conventional exec-driven Cast To... node). Setting pure=true
        // gives a pure data-only cast with no exec pins, useful inside
        // binding/pure functions.
        bool bPureCast = false;
        Context.Payload->TryGetBoolField(TEXT("pure"), bPureCast);
        CastNode->SetPurity(bPureCast);
        Context.FinalizeNode(CastCreator, CastNode, X, Y);
        return;
    }

    // UK2Node_ConstructObjectFromClass and its subclasses (SpawnActorFromClass,
    // ConstructObjectFromClass, ...) hard-crash the editor on the generic path
    // below: their PostPlacedNewNode() dereferences a checked pin accessor (e.g.
    // UK2Node_SpawnActorFromClass::GetScaleMethodPin() -> FindPinChecked) before
    // AllocateDefaultPins() has created any pins. They need pins allocated first.
    // CreateWidget is one of them (its header is private to UMGEditor).
    if (TryCreateConstructObjectNode(Context, NodeClass, X, Y))
    {
        return;
    }

    // Make/Break struct nodes build their pins from StructType. Spawned by class name they came out
    // as a pinless "Make <unknown struct>" while the batch answered success; structPath names the
    // struct (a Blueprint Struct asset or a native one such as /Script/SlateCore.SlateColor).
    UScriptStruct* StructType = nullptr;
    if (NodeClass->IsChildOf(UK2Node_StructOperation::StaticClass()))
    {
        FString StructPath;
        Context.Payload->TryGetStringField(TEXT("structPath"), StructPath);
        StructType = StructPath.IsEmpty() ? nullptr : LoadObject<UScriptStruct>(nullptr, *StructPath);
        if (!StructType && !StructPath.IsEmpty() && !StructPath.Contains(TEXT(".")))
        {
            StructType = LoadObject<UScriptStruct>(nullptr, *(StructPath + TEXT(".") + FPackageName::GetShortName(StructPath)));
        }
        if (!StructType)
        {
            Context.SendError(
                FString::Printf(TEXT("%s needs structPath: a Blueprint Struct asset or a native struct such as "
                                     "/Script/SlateCore.SlateColor%s."),
                                *NodeType, StructPath.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("; '%s' was not found"), *StructPath)),
                TEXT("INVALID_ARGUMENT"));
            return;
        }
        // A struct with a native make or break (Vector, Rotator, Transform, LinearColor ...) is made and
        // broken by that function, as the editor's menu does: the generic node compiled with "The structure
        // cannot be broken using generic 'break' node".
        const FString& Native = StructType->GetMetaData(
            NodeClass->IsChildOf(UK2Node_MakeStruct::StaticClass()) ? TEXT("HasNativeMake") : TEXT("HasNativeBreak"));
        if (UFunction* NativeFunction = Native.IsEmpty() ? nullptr : FindObject<UFunction>(nullptr, *Native))
        {
            FGraphNodeCreator<UK2Node_CallFunction> NativeCreator(*Context.TargetGraph);
            UK2Node_CallFunction* NativeNode = NativeCreator.CreateNode(false);
            NativeNode->SetFromFunction(NativeFunction);
            Context.FinalizeNode(NativeCreator, NativeNode, X, Y);
            return;
        }
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
    if (UK2Node_StructOperation* StructNode = Cast<UK2Node_StructOperation>(NewNode))
    {
        StructNode->StructType = StructType;
    }

    Context.TargetGraph->AddNode(NewNode, false, false);
    NewNode->CreateNewGuid();
    // ROOT-CAUSE FIX (mirrors ConstructObjectNodes): allocate pins BEFORE
    // PostPlacedNewNode(). Node families such as UK2Node_SpawnActorFromClass read
    // checked pin accessors inside PostPlacedNewNode() (GetScaleMethodPin() =>
    // FindPinChecked()), which check()-asserts the editor when the pin list is
    // still empty (EdGraphNode.h:586). Allocating first makes those accessors safe;
    // the guard below still avoids duplicating pins for nodes that allocate their
    // own inside PostPlacedNewNode() (e.g. UK2Node_FunctionResult).
    if (NewNode->Pins.Num() == 0)
    {
        NewNode->AllocateDefaultPins();
    }
    NewNode->PostPlacedNewNode();
    NewNode->NodePosX = X;
    NewNode->NodePosY = Y;
    // Refuse stacked placements: estimate from the allocated pins and pull the
    // node back out on overlap, failing with coordinates + free slots.
    {
        FString OverlapMessage;
        TSharedPtr<FJsonObject> OverlapDetails;
        if (McpGraphLayout::RefuseOverlappingNode(Context.TargetGraph, NewNode, X, Y, OverlapMessage, OverlapDetails))
        {
            Context.SendErrorWithDetails(OverlapMessage, TEXT("NODE_OVERLAP"), OverlapDetails);
            return;
        }
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(Context.Blueprint);
    SaveLoadedAssetThrottled(Context.Blueprint);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    // `nodeGuid` is the required output field on blueprint.create_node; see the
    // note in McpAutomationBridge_BlueprintGraphHandlersPrivate.h.
    Result->SetStringField(TEXT("nodeGuid"), NewNode->NodeGuid.ToString());
    Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
    Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
    Result->SetStringField(TEXT("nodeClass"), NodeClass->GetName());
    Context.SendResponse(TEXT("Node created."), Result);
}
}

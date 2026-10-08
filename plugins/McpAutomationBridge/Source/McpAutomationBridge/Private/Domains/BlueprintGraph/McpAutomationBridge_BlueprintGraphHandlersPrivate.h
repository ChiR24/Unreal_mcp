#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UObjectIterator.h"
#include "Foundation/GraphLayout/McpGraphNodeExtent.h"

class UK2Node_CallFunction;

namespace McpBlueprintGraphHandlers
{
// "Pin not found." named neither the pin looked for nor what would have
// worked, so every miss cost a separate inspect_graph round trip. The pins are
// already in hand at each of those sites; this names them.
static inline FString DescribeNodePins(UEdGraphNode* Node)
{
    if (!Node)
    {
        return TEXT("<node not found>");
    }
    TArray<FString> Names;
    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin)
        {
            Names.Add(FString::Printf(TEXT("%s (%s)"), *Pin->GetName(),
                Pin->Direction == EGPD_Output ? TEXT("out") : TEXT("in")));
        }
    }
    return Names.Num() > 0 ? FString::Join(Names, TEXT(", ")) : TEXT("<none>");
}

// The classes other than Class that declare a Blueprint-callable Wanted, as a
// retry hint; empty when none does.
FString DescribeDeclaringClasses(UClass* Class, const FString& Wanted);

// "Function 'X' not found" named nothing that WOULD work, so every miss cost a
// round trip of guessing. Two things are worth saying: the reflected names that
// look like what was asked for, and - the case that bites hardest - that the
// name is a PROPERTY, which is a VariableGet/VariableSet node, not a function.
static inline FString SuggestMemberFix(UClass* Class, const FString& Wanted)
{
    if (!Class || Wanted.IsEmpty())
    {
        return FString();
    }
    FString Bare = Wanted;
    if (Bare.StartsWith(TEXT("Set")) || Bare.StartsWith(TEXT("Get")))
    {
        Bare = Bare.RightChop(3);
    }
    for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt)
    {
        const FString PropName = PropIt->GetName();
        if (PropName.Equals(Wanted, ESearchCase::IgnoreCase) ||
            PropName.Equals(Bare, ESearchCase::IgnoreCase) ||
            PropName.Equals(TEXT("b") + Bare, ESearchCase::IgnoreCase))
        {
            return FString::Printf(
                TEXT(" '%s' is a PROPERTY on %s, not a function - create it with "
                     "nodeType VariableGet or VariableSet and memberName '%s'."),
                *PropName, *Class->GetName(), *PropName);
        }
    }
    // Underscores do not count: VInterpToConstant asked for VInterpTo_Constant and was told nothing.
    const FString WantedKey = Wanted.Replace(TEXT("_"), TEXT("")).ToLower();
    const FString BareKey = Bare.Replace(TEXT("_"), TEXT("")).ToLower();
    TArray<FString> Close;
    for (TFieldIterator<UFunction> FuncIt(Class); FuncIt; ++FuncIt)
    {
        const FString Name = FuncIt->GetName();
        const FString Key = Name.Replace(TEXT("_"), TEXT("")).ToLower();
        if (Key.Contains(WantedKey) ||
            (Bare.Len() >= 4 && Key.Contains(BareKey)))
        {
            Close.AddUnique(Name);
        }
    }
    if (Close.Num() == 0)
    {
        return DescribeDeclaringClasses(Class, Wanted);
    }
    Close.Sort();
    if (Close.Num() > 8)
    {
        Close.SetNum(8);
    }
    return FString::Printf(TEXT(" Closest reflected names on %s: %s."),
                           *Class->GetName(), *FString::Join(Close, TEXT(", ")));
}

struct FActionContext
{
    UMcpAutomationBridgeSubsystem* Subsystem = nullptr;
    FString RequestId;
    TSharedPtr<FJsonObject> Payload;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
    FString SubAction;
    UBlueprint* Blueprint = nullptr;
    UEdGraph* TargetGraph = nullptr;
    // No compile at response time: a build_graph step (half-wired states are
    // expected mid-batch, so the batch compiles once at the end) or a read.
    bool bDeferCompile = false;

    void SendError(const FString& Message, const FString& ErrorCode) const;
    void SendErrorWithDetails(
        const FString& Message,
        const FString& ErrorCode,
        const TSharedPtr<FJsonObject>& Details) const;
    void SendResponse(
        const FString& Message,
        const TSharedPtr<FJsonObject>& Result) const;
    // Names the Blueprint the edit ran on (blueprintPath, unless the reply already carries its assetPath), so the
    // receipt has its handle. bChanged also lists it under changedAssets, which the receipt reads as changed.
    void NameBlueprint(const TSharedPtr<FJsonObject>& Result, bool bChanged) const;

    UEdGraphNode* FindNode(const FString& Id) const;
    void SendNodeNotFound(const FString& Id) const;
    UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinName) const;

    template <typename NodeCreatorType, typename NodeType>
    void FinalizeNode(
        NodeCreatorType& NodeCreator,
        NodeType* NewNode,
        float X,
        float Y) const
    {
        if (!NewNode)
        {
            SendError(
                TEXT("Failed to create node (unsupported type or internal error)."),
                TEXT("CREATE_FAILED"));
            return;
        }

        NewNode->NodePosX = X;
        NewNode->NodePosY = Y;
        NodeCreator.Finalize();
        // Refuse stacked placements: the node is already in the graph at this
        // point (FGraphNodeCreator adds it on CreateNode), so pull it back out
        // on overlap and fail with coordinates instead of silently stacking.
        {
            float NewWidth = 0.0f;
            float NewHeight = 0.0f;
            McpGraphLayout::EstimateNodeExtent(*NewNode, NewWidth, NewHeight);
            TArray<McpGraphLayout::FGraphNodeOccupant> Overlapping;
            bool bOverlaps = McpGraphLayout::CheckGraphNodeOverlap(
                TargetGraph, X, Y, NewWidth, NewHeight, Overlapping,
                McpGraphLayout::NodeOverlapPadding, NewNode);
            // A call that named no position gets the first free slot to the
            // right instead of an overlap refusal it could only answer by retrying.
            const bool bAutoPlace = Payload.IsValid() &&
                !Payload->HasField(TEXT("x")) && !Payload->HasField(TEXT("posX")) &&
                !Payload->HasField(TEXT("y")) && !Payload->HasField(TEXT("posY"));
            for (int32 Step = 0; bOverlaps && bAutoPlace && Step < 8; ++Step)
            {
                for (const McpGraphLayout::FGraphNodeOccupant& Occupant : Overlapping)
                {
                    X = FMath::Max(X, Occupant.X + Occupant.Width + McpGraphLayout::NodeSuggestGap);
                }
                NewNode->NodePosX = X;
                bOverlaps = McpGraphLayout::CheckGraphNodeOverlap(
                    TargetGraph, X, Y, NewWidth, NewHeight, Overlapping,
                    McpGraphLayout::NodeOverlapPadding, NewNode);
            }
            if (bOverlaps)
            {
                TargetGraph->RemoveNode(NewNode);
                FString OverlapMessage;
                TSharedPtr<FJsonObject> OverlapDetails =
                    McpGraphLayout::BuildNodeOverlapDetails(
                        X, Y, NewWidth, NewHeight, Overlapping, OverlapMessage, TargetGraph);
                SendErrorWithDetails(OverlapMessage, TEXT("NODE_OVERLAP"), OverlapDetails);
                return;
            }
        }
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        SaveLoadedAssetThrottled(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        // `nodeGuid` is the name blueprint.create_node/add_event declare and
        // mark REQUIRED in their output schema. The gateway projects the result
        // to schema-declared names only, so emitting just `nodeId` projected to
        // an empty payload and turned every successful node creation into
        // OUTPUT_SCHEMA_VIOLATION — the node existed in the graph while the
        // caller was told it failed. Both names are emitted: `nodeGuid` for the
        // canonical contract, `nodeId` for the WebSocket surface and for
        // delete_node/connect_pins, which consume that spelling.
        const FString NodeGuidText = NewNode->NodeGuid.ToString();
        Result->SetStringField(TEXT("nodeGuid"), NodeGuidText);
        Result->SetStringField(TEXT("nodeId"), NodeGuidText);
        Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
        // Callers place nodes by coordinate but were told nothing back about
        // where the node actually landed or how big it is, so consecutive
        // creates silently stacked on top of each other.
        const FString PlacementWarning =
            McpGraphLayout::AddNodePlacementFields(Result, *NewNode);
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendResponse(
            PlacementWarning.IsEmpty()
                ? FString(TEXT("Node created."))
                : FString::Printf(TEXT("Node created. %s"), *PlacementWarning),
            Result);
    }
};

bool ValidateProvidedPaths(const FActionContext& Context);
bool PrepareBlueprintAndGraph(FActionContext& Context);
bool HandleListNodeTypes(FActionContext& Context);
bool HandleNodeCreationAction(FActionContext& Context);
bool HandlePinMutationAction(FActionContext& Context);
bool SetPinDefaultValue(FActionContext& Context);
// A read-only (const reference or required) pin's literal, set on a MakeLiteral
// node wired into it; sends the reply. RemoveNodeWithLiterals drops a node and
// the MakeLiteral nodes feeding only it.
bool FeedReadOnlyPinLiteral(FActionContext& Context, UEdGraphNode& TargetNode, UEdGraphPin& Pin, const FString& Value);
void RemoveNodeWithLiterals(UBlueprint* Blueprint, UEdGraphNode* Node);
bool HandleNodeMutationAction(FActionContext& Context);
// Sets a reflected node field (e.g. an AnimGraph player's Sequence/BlendSpace)
// by name, loading an asset path for object properties.
bool McpTrySetNodeAssetPropertyForMcp(UEdGraphNode* TargetNode,
                                      const FString& PropertyName,
                                      const FString& Value);
bool HandleNodeQueryAction(FActionContext& Context);
bool HandleNodeDetailAction(FActionContext& Context);
// build_graph: runs a list of the single-step edits above in one request.
bool HandleGraphBatchAction(FActionContext& Context);

// The function a CallFunction step names: on memberClass (else the one library
// declaring it), or on the Blueprint's own class and the stock libraries. Shared
// by node creation and the build_graph pre-check so the two cannot disagree.
UFunction* ResolveGraphCallFunction(UBlueprint* Blueprint, const FString& MemberName,
                                    const FString& MemberClass, UClass*& OutResolvedClass);
// A call node's function from memberClass and targetClass; OutOutputClass is the class a DeterminesOutputType
// pin (GetAllActorsOfClass's ActorClass) takes, OutOwnerClass the class the function was looked up on.
UFunction* ResolveCallNodeFunction(UBlueprint* Blueprint, const TSharedPtr<FJsonObject>& Payload, const FString& MemberName,
                                   FString& OutOwnerClass, FString& OutOutputClass, UClass*& OutResolvedClass);
// That class, checked against the pin before any node exists (bOutRefused: the error is sent), and set on the new
// node so its result pin takes the class.
UClass* ResolveOutputClass(FActionContext& Context, const UFunction& Function, const FString& ClassText, bool& bOutRefused);
void ApplyOutputClass(UK2Node_CallFunction& Node, const UFunction& Function, UClass* OutputClass);
// Why a CallFunction name did not resolve: an unresolved memberClass, else the member hint.
FString DescribeMissingFunction(UBlueprint* Blueprint, const FString& MemberName,
                                const FString& MemberClass, UClass* ResolvedClass);
const TTuple<FString, FString>* FindCommonFunctionNode(const FString& NodeType);
UClass* FindNodeClassByName(const FString& NodeType);
// The StandardMacros graph a nodeType names (ForLoop, DoOnce, DoN -> "Do N" ...), or null.
const FString* StandardMacroGraphName(const FString& NodeType);
// Resolve a class string (Blueprint asset path like /Game/..., generated-class
// path, or native class name) to a UClass. Shared so every create_node branch
// with a class pin accepts the same input formats — including /Game/ Blueprint
// paths, which the name-oriented ResolveUClass() cannot load.
UClass* ResolveTargetClassFromString(const FString& InClassString);
FEdGraphPinType ResolveCustomEventPinType(const FString& TypeName);
FProperty* CreateCustomEventParameter(
    UFunction* Function,
    const FString& ParameterName,
    const FString& ParameterType);

bool TryCreateCommonFunctionNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y);
// VariableGet, GetVariable or K2Node_VariableGet, any case (the Set forms set
// bOutIsSet). Shared by create_node and the build_graph pre-check.
bool ParseVariableNodeType(const FString& NodeType, bool& bOutIsSet);
bool TryCreateVariableNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y);
bool TryCreateFunctionOrEventNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y);
bool TryCreateCustomEventNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y);
bool TryCreateSpecialNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y);
bool TryCreateEnhancedInputNode(
    FActionContext& Context,
    UClass* NodeClass,
    float X,
    float Y);
bool TryCreateConstructObjectNode(
    FActionContext& Context,
    UClass* NodeClass,
    float X,
    float Y);
bool TryCreateSubsystemNode(
    FActionContext& Context,
    UClass* NodeClass,
    float X,
    float Y);
void CreateDynamicNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y);
}

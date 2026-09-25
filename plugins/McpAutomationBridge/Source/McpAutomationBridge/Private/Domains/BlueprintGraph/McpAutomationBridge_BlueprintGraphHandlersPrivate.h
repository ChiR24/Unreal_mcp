#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Core/Module/McpAutomationBridgeGlobals.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UObjectIterator.h"
#include "Foundation/GraphLayout/McpGraphNodeExtent.h"
#endif

namespace McpBlueprintGraphHandlers
{
#if WITH_EDITOR
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
    const FString WantedLower = Wanted.ToLower();
    TArray<FString> Close;
    for (TFieldIterator<UFunction> FuncIt(Class); FuncIt; ++FuncIt)
    {
        const FString Name = FuncIt->GetName();
        const FString Lower = Name.ToLower();
        if (Lower.Contains(WantedLower) ||
            (Bare.Len() >= 4 && Lower.Contains(Bare.ToLower())))
        {
            Close.AddUnique(Name);
        }
    }
    if (Close.Num() == 0)
    {
        // Nothing on the class asked about, but the name may be exactly right
        // and living on a DIFFERENT Blueprint function library -- the engine
        // splits closely related helpers across libraries with no hint in the
        // naming (RemoveAllWidgets is on UWidgetLayoutLibrary while every
        // neighbouring widget helper is on UWidgetBlueprintLibrary). A bare
        // "Function not found" sends the caller hunting the wrong class, so
        // name the one that actually declares it.
        const FName WantedName(*Wanted);
        for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
        {
            UClass* Candidate = *ClassIt;
            if (Candidate == Class ||
                !Candidate->IsChildOf(UBlueprintFunctionLibrary::StaticClass()))
            {
                continue;
            }
            const UFunction* Found = Candidate->FindFunctionByName(WantedName);
            if (Found && Found->HasAnyFunctionFlags(FUNC_BlueprintCallable))
            {
                return FString::Printf(
                    TEXT(" '%s' is not on %s, but %s declares it - retry with "
                         "memberClass '%s'."),
                    *Wanted, *Class->GetName(), *Candidate->GetName(),
                    *Candidate->GetPathName());
            }
        }
        return FString();
    }
    Close.Sort();
    if (Close.Num() > 8)
    {
        Close.SetNum(8);
    }
    return FString::Printf(TEXT(" Closest reflected names on %s: %s."),
                           *Class->GetName(), *FString::Join(Close, TEXT(", ")));
}
#endif

struct FActionContext
{
    UMcpAutomationBridgeSubsystem* Subsystem = nullptr;
    FString RequestId;
    TSharedPtr<FJsonObject> Payload;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
    FString SubAction;
#if WITH_EDITOR
    UBlueprint* Blueprint = nullptr;
    UEdGraph* TargetGraph = nullptr;
#endif
    // A build_graph step: half-wired states are expected mid-batch, so the
    // batch compiles once at the end instead of after every step.
    bool bDeferCompile = false;

    void SendError(const FString& Message, const FString& ErrorCode) const;
    void SendErrorWithDetails(
        const FString& Message,
        const FString& ErrorCode,
        const TSharedPtr<FJsonObject>& Details) const;
    void SendResponse(
        const FString& Message,
        const TSharedPtr<FJsonObject>& Result) const;

#if WITH_EDITOR
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
                        X, Y, NewWidth, NewHeight, Overlapping, OverlapMessage);
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
#endif
};

bool ValidateProvidedPaths(const FActionContext& Context);
bool PrepareBlueprintAndGraph(FActionContext& Context);
bool HandleListNodeTypes(FActionContext& Context);
bool HandleNodeCreationAction(FActionContext& Context);
bool HandlePinMutationAction(FActionContext& Context);
bool SetPinDefaultValue(FActionContext& Context);
FString PickFirstNonEmpty(const TSharedPtr<FJsonObject>& Payload, const TArray<const TCHAR*>& Keys);
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

#if WITH_EDITOR
// The function a CallFunction step names: on memberClass (else the one library
// declaring it), or on the Blueprint's own class and the stock libraries. Shared
// by node creation and the build_graph pre-check so the two cannot disagree.
UFunction* ResolveGraphCallFunction(UBlueprint* Blueprint, const FString& MemberName,
                                    const FString& MemberClass, UClass*& OutResolvedClass);
const TTuple<FString, FString>* FindCommonFunctionNode(const FString& NodeType);
UClass* FindNodeClassByName(const FString& NodeType);
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
#endif
}

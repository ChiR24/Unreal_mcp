#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPActorBinding.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UObjectIterator.h"
#include "Foundation/GraphLayout/McpGraphOverlap.h"

class UK2Node_CallFunction;
class UK2Node_Variable;

namespace McpBlueprintGraphHandlers
{

// The classes other than Class that declare a Blueprint-callable Wanted, as a
// retry hint; empty when none does.
FString DescribeDeclaringClasses(UClass* Class, const FString& Wanted);

// "Function 'X' not found" named nothing that WOULD work, so every miss cost a
// round trip of guessing. Two things are worth saying: the reflected names that
// look like what was asked for, and - the case that bites hardest - that the
// name is a PROPERTY, which is a VariableGet/VariableSet node, not a function.
FString SuggestMemberFix(UClass* Class, const FString& Wanted);

struct FActionContext
{
    UEngineWireSubsystem* Subsystem = nullptr;
    FString RequestId;
    TSharedPtr<FJsonObject> Payload;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket;
    FString SubAction;
    UBlueprint* Blueprint = nullptr;
    UEdGraph* TargetGraph = nullptr;
    // No compile at response time: a build_graph step (half-wired states are
    // expected mid-batch, so the batch compiles once at the end) or a read.
    bool bDeferCompile = false;
    // When set, a success reply goes here instead of to the socket: the level Blueprint door runs a graph edit on
    // its level Blueprint and answers through its own level-save rule.
    TFunction<void(const FString&, const TSharedPtr<FJsonObject>&)> OnSuccess;

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
        FinishCreatedNode(NewNode, X, Y);
    }

    // The node is in the graph at (X, Y): refuse it on an overlap (pulled back out, with coordinates), else mark the
    // Blueprint changed, save and reply with nodeGuid and where it landed.
    void FinishCreatedNode(UEdGraphNode* NewNode, float X, float Y) const;
};

bool ValidateProvidedPaths(const FActionContext& Context);
// Loads the Blueprint and the graph the payload names; bRequireGraph false lets a read of every graph go on without one.
bool PrepareBlueprintAndGraph(FActionContext& Context, bool bRequireGraph = true);
bool HandleListNodeTypes(FActionContext& Context);
bool HandleFindFunctions(FActionContext& Context);
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

// CallFunction, K2Node_CallFunction or FunctionCall: the nodeType node creation and the build_graph pre-check both
// read as a function call.
bool IsCallFunctionType(const FString& NodeType);
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
UClass* FindNodeClassByName(const FString& NodeType);
// Empty when a node of NodeClass can go in Graph; else why not, for a NODE_NOT_ALLOWED_IN_GRAPH refusal.
FString DescribeGraphMismatch(UClass* NodeClass, const UEdGraph* Graph);
// The StandardMacros graph a nodeType names (ForLoop, DoOnce, DoN -> "Do N" ...), or null.
const FString* StandardMacroGraphName(const FString& NodeType);
// pinSubType, container and a map's valueType on a pin descriptor (McpBPGraphQueries.cpp).
void AddPinTypeFields(const TSharedPtr<FJsonObject>& PinObject, const FEdGraphPinType& PinType);
// One pin as JSON for every graph read: pinName, direction, pinType and its type fields, linkedTo (each far node's id,
// pin and title) and the default fields (McpBPGraphQueries.cpp).
TSharedPtr<FJsonObject> MakePinObject(UEdGraphPin* Pin);
// defaultValue / defaultTextValue / defaultObjectPath, only on a visible input nothing is wired to.
void AddPinDefaultFields(const TSharedPtr<FJsonObject>& PinObject, const UEdGraphPin& Pin);

bool TryCreateCommonFunctionNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y);
// VariableGet, GetVariable or K2Node_VariableGet, any case (the Set forms set
// bOutIsSet). Shared by create_node and the build_graph pre-check.
bool ParseVariableNodeType(const FString& NodeType, bool& bOutIsSet);
// The class a memberClass names: a native class name, a class path or a Blueprint asset path; null when none.
UClass* ResolveMemberClass(const FString& MemberClassName);
// The property a variable node names: VariableName on MemberClassName when given (a placed actor's component is
// PointLightComponent on PointLight), else Blueprint's own variable (bOutOwnVariable, no property yet) or one its
// generated, skeleton or parent class has. Null and no own variable when nothing matches; OutOwner is then
// memberClass's class (null when it names none), the class a caller can list members of.
FProperty* ResolveVariableMember(UBlueprint* Blueprint, FName VariableName, const FString& MemberClassName,
                                 UClass*& OutOwner, bool& bOutOwnVariable);
// Points Node at it, before its pins exist: a self member when Blueprint's class has it, else a member of Owner,
// read from the object wired into the node's Target pin. Shared by create_node, add_node and the level Blueprint.
void SetVariableReference(UK2Node_Variable& Node, UBlueprint* Blueprint, FProperty* Property, UClass* Owner,
                          bool bOwnVariable, FName VariableName);
// Why Blueprint cannot read (or, bWrite, write) Property, as its compile would refuse it: not Blueprint-visible,
// private to another Blueprint, or read-only. Empty when it can, and for a null Property (an uncompiled own variable).
FString DescribeVariableAccess(const UBlueprint* Blueprint, const FProperty* Property, bool bWrite);
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
// A cast node for every spelling that asks for one (Cast, CastTo<Class>, DynamicCast, K2Node_DynamicCast): the class from
// targetClass (or memberClass, nodeClass, widgetType, or the CastTo suffix), `pure` read on each. Replies on its own.
void CreateCastNode(FActionContext& Context, const FString& NodeType, float X, float Y);
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
// Switch on Enum and the other nodes that build their pins from an Enum property, seeded from enumPath.
bool TryCreateEnumNode(FActionContext& Context, UClass* NodeClass, float X, float Y);
// A level Blueprint's actor-bound event or actor reference, bound to actorName (and eventName) before its pins exist.
bool TryCreateLevelActorNode(FActionContext& Context, UClass* NodeClass, float X, float Y);
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

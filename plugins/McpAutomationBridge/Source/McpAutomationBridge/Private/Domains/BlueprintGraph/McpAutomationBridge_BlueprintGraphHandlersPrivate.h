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
#include "Kismet2/BlueprintEditorUtils.h"
#include "Foundation/GraphLayout/McpGraphNodeExtent.h"
#endif

namespace McpBlueprintGraphHandlers
{
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

    void SendError(const FString& Message, const FString& ErrorCode) const;
    void SendResponse(
        const FString& Message,
        const TSharedPtr<FJsonObject>& Result) const;

#if WITH_EDITOR
    UEdGraphNode* FindNode(const FString& Id) const;
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
bool HandleNodeQueryAction(FActionContext& Context);
bool HandleNodeDetailAction(FActionContext& Context);

#if WITH_EDITOR
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
void CreateDynamicNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y);
#endif
}

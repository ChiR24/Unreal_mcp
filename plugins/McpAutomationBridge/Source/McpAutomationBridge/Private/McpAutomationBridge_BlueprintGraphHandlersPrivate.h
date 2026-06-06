#pragma once

#include "McpVersionCompatibility.h"

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Blueprint.h"
#include "K2Node.h"
#include "Kismet2/BlueprintEditorUtils.h"
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
        Result->SetStringField(TEXT("nodeId"), NewNode->NodeGuid.ToString());
        Result->SetStringField(TEXT("nodeName"), NewNode->GetName());
        McpHandlerUtils::AddVerification(Result, Blueprint);
        SendResponse(TEXT("Node created."), Result);
    }
#endif
};

bool ValidateProvidedPaths(const FActionContext& Context);
bool PrepareBlueprintAndGraph(FActionContext& Context);
bool HandleListNodeTypes(FActionContext& Context);
bool HandleNodeCreationAction(FActionContext& Context);
bool HandlePinMutationAction(FActionContext& Context);
bool HandleNodeMutationAction(FActionContext& Context);
bool HandleNodeQueryAction(FActionContext& Context);
bool HandleNodeDetailAction(FActionContext& Context);

#if WITH_EDITOR
const TTuple<FString, FString>* FindCommonFunctionNode(const FString& NodeType);
UClass* FindNodeClassByName(const FString& NodeType);
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
void CreateDynamicNode(
    FActionContext& Context,
    const FString& NodeType,
    float X,
    float Y);
#endif
}

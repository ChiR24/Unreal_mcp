#include "McpVersionCompatibility.h"
#include "McpAutomationBridge_MiscHandlersSupport.h"
#include "McpHandlerUtils.h"

#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "Dom/JsonObject.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"

#if WITH_EDITOR
namespace McpMiscHandlers
{
bool HandleCreateRPC(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"), TEXT(""));
    FString FunctionName = GetStringField(Payload, TEXT("functionName"), TEXT(""));
    FString RPCType = GetStringField(Payload, TEXT("rpcType"), TEXT("Server"));
    bool bReliable = GetBoolField(Payload, TEXT("reliable"), true);

    if (BlueprintPath.IsEmpty() || FunctionName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, TEXT("blueprintPath and functionName are required"), nullptr, TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), nullptr, TEXT("NOT_FOUND"));
        return true;
    }

    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        FName(*FunctionName),
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass());

    if (NewGraph)
    {
        FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, NewGraph, false, static_cast<UFunction*>(nullptr));
        for (UEdGraphNode* Node : NewGraph->Nodes)
        {
            if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
            {
                int32 NetFlags = FUNC_Net;
                if (bReliable)
                {
                    NetFlags |= FUNC_NetReliable;
                }
                if (RPCType.Equals(TEXT("Server"), ESearchCase::IgnoreCase))
                {
                    NetFlags |= FUNC_NetServer;
                }
                else if (RPCType.Equals(TEXT("Client"), ESearchCase::IgnoreCase))
                {
                    NetFlags |= FUNC_NetClient;
                }
                else if (RPCType.Equals(TEXT("Multicast"), ESearchCase::IgnoreCase) ||
                         RPCType.Equals(TEXT("NetMulticast"), ESearchCase::IgnoreCase))
                {
                    NetFlags |= FUNC_NetMulticast;
                }
                EntryNode->AddExtraFlags(NetFlags);
                break;
            }
        }

        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    ResponseJson->SetStringField(TEXT("functionName"), FunctionName);
    ResponseJson->SetStringField(TEXT("rpcType"), RPCType);
    ResponseJson->SetBoolField(TEXT("reliable"), bReliable);
    if (NewGraph)
    {
        McpHandlerUtils::AddVerification(ResponseJson, Blueprint);
    }

    Subsystem->SendAutomationResponse(Socket, RequestId, NewGraph != nullptr,
        NewGraph ? FString::Printf(TEXT("Created %s RPC: %s"), *RPCType, *FunctionName) : TEXT("Failed to create RPC"),
        ResponseJson);
    return true;
}
}
#endif

#include "McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
bool HandleCreateRpcFunction(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
    FString FunctionName = GetStringField(Payload, TEXT("functionName"));
    FString RpcType = GetStringField(Payload, TEXT("rpcType"));
    bool bReliable = GetBoolField(Payload, TEXT("reliable"), true);

    if (BlueprintPath.IsEmpty() || FunctionName.IsEmpty() || RpcType.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
    if (!Blueprint)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
        return true;
    }

    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(
        Blueprint,
        FName(*FunctionName),
        UEdGraph::StaticClass(),
        UEdGraphSchema_K2::StaticClass());

    if (!NewGraph)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Failed to create function graph"), TEXT("CREATE_FAILED"));
        return true;
    }

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
            if (RpcType.Equals(TEXT("Server"), ESearchCase::IgnoreCase))
            {
                NetFlags |= FUNC_NetServer;
            }
            else if (RpcType.Equals(TEXT("Client"), ESearchCase::IgnoreCase))
            {
                NetFlags |= FUNC_NetClient;
            }
            else if (RpcType.Equals(TEXT("NetMulticast"), ESearchCase::IgnoreCase) || RpcType.Equals(TEXT("Multicast"), ESearchCase::IgnoreCase))
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

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("functionName"), FunctionName);
    ResultJson->SetStringField(TEXT("rpcType"), RpcType);
    ResultJson->SetBoolField(TEXT("reliable"), bReliable);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Created %s RPC function: %s"), *RpcType, *FunctionName));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("RPC function created"), ResultJson);
    return true;
}
}

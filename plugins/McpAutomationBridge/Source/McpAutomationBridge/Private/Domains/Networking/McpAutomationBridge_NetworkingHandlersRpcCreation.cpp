#include "Domains/Networking/McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
bool HandleCreateRpcFunction(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString FunctionName = GetJsonStringField(Payload, TEXT("functionName"));
    FString RpcType = GetJsonStringField(Payload, TEXT("rpcType"));
    bool bReliable = GetJsonBoolField(Payload, TEXT("reliable"), true);

    if (BlueprintPath.IsEmpty() || FunctionName.IsEmpty() || RpcType.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
        return true;
    }
    // An unknown rpcType used to create a FUNC_Net function with no Server, Client or Multicast
    // flag (not an RPC at all) and still reply "Created <rpcType> RPC".
    int32 DirectionFlag = 0;
    if (RpcType.Equals(TEXT("Server"), ESearchCase::IgnoreCase))
    {
        DirectionFlag = FUNC_NetServer;
    }
    else if (RpcType.Equals(TEXT("Client"), ESearchCase::IgnoreCase))
    {
        DirectionFlag = FUNC_NetClient;
    }
    else if (RpcType.Equals(TEXT("NetMulticast"), ESearchCase::IgnoreCase) || RpcType.Equals(TEXT("Multicast"), ESearchCase::IgnoreCase))
    {
        DirectionFlag = FUNC_NetMulticast;
    }
    else
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            FString::Printf(TEXT("Unknown rpcType '%s'; use Server, Client, or NetMulticast. Nothing was created."), *RpcType),
            TEXT("INVALID_ARGUMENT"));
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
            NetFlags |= DirectionFlag;
            if (bReliable)
            {
                NetFlags |= FUNC_NetReliable;
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

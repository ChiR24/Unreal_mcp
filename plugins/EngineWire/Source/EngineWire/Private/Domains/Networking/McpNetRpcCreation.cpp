#include "Domains/Networking/McpNetPrivate.h"

#include "Components/ActorComponent.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsBlueprintGraph.h"

namespace McpNetworkingHandlers
{
namespace
{
// Server, Client or NetMulticast ("Multicast" too, any case) -> its direction flag, with the canonical name in
// OutName; 0 for anything else. An unknown rpcType used to create a FUNC_Net function with no direction flag
// (not an RPC at all) and still reply "Created <rpcType> RPC".
int32 ParseRpcDirection(const FString& RpcType, FString& OutName)
{
    if (RpcType.Equals(TEXT("Server"), ESearchCase::IgnoreCase))
    {
        OutName = TEXT("Server");
        return FUNC_NetServer;
    }
    if (RpcType.Equals(TEXT("Client"), ESearchCase::IgnoreCase))
    {
        OutName = TEXT("Client");
        return FUNC_NetClient;
    }
    if (RpcType.Equals(TEXT("NetMulticast"), ESearchCase::IgnoreCase) || RpcType.Equals(TEXT("Multicast"), ESearchCase::IgnoreCase))
    {
        OutName = TEXT("NetMulticast");
        return FUNC_NetMulticast;
    }
    return 0;
}
}

bool HandleCreateRpcFunction(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    const FString FunctionName = GetJsonStringField(Payload, TEXT("functionName")).TrimStartAndEnd();
    const FString RpcType = GetJsonStringField(Payload, TEXT("rpcType"));
    const bool bReliable = GetJsonBoolField(Payload, TEXT("reliable"), true);

    if (BlueprintPath.IsEmpty() || FunctionName.IsEmpty() || RpcType.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("blueprintPath, functionName and rpcType are required"), TEXT("INVALID_PARAMS"));
        return true;
    }
    FString RpcName;
    const int32 DirectionFlag = ParseRpcDirection(RpcType, RpcName);
    if (DirectionFlag == 0)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            FString::Printf(TEXT("Unknown rpcType '%s'; use Server, Client, or NetMulticast. Nothing was created."), *RpcType),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const TArray<TSharedPtr<FJsonValue>>* InputsField = nullptr;
    Payload->TryGetArrayField(TEXT("inputs"), InputsField);
    const TArray<TSharedPtr<FJsonValue>> Inputs = InputsField ? *InputsField : TArray<TSharedPtr<FJsonValue>>();
    const FString BadInput = McpBlueprintUtils::DescribeInvalidParam(Inputs, TEXT("inputs"));
    if (!BadInput.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, BadInput, TEXT("TYPE_RESOLUTION_FAILED"));
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
    if (!Blueprint)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
        return true;
    }
    // A call only crosses the network through an actor's connection (a component's through its owner); on any
    // other class GetFunctionCallspace is local, so the "RPC" would run where it was called, every time.
    UClass* ParentClass = Blueprint->ParentClass;
    if (!ParentClass || !(ParentClass->IsChildOf(AActor::StaticClass()) || ParentClass->IsChildOf(UActorComponent::StaticClass())))
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            FString::Printf(TEXT("%s is not an Actor or ActorComponent Blueprint; an RPC on it would never leave the machine that calls it. Nothing was created."), *Blueprint->GetName()),
            TEXT("INVALID_TARGET"));
        return true;
    }
    if (!CheckFreeMemberNameOrReply(Context, Blueprint, FunctionName, TEXT("functionName")))
    {
        return true;
    }

    Blueprint->Modify();
    UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FName(*FunctionName), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
    if (!NewGraph)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Failed to create function graph"), TEXT("CREATE_FAILED"));
        return true;
    }
    // User-created, as the editor's own new functions: an editable entry (so parameters can be added there later)
    // and the public, callable, overridable flags. Without it the function had none of them.
    FBlueprintEditorUtils::AddFunctionGraph<UFunction>(Blueprint, NewGraph, true, static_cast<UFunction*>(nullptr));

    for (UEdGraphNode* Node : NewGraph->Nodes)
    {
        if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
        {
            EntryNode->AddExtraFlags(FUNC_Net | DirectionFlag | (bReliable ? FUNC_NetReliable : 0));
            for (const TSharedPtr<FJsonValue>& Input : Inputs)
            {
                const TSharedPtr<FJsonObject> Pin = Input->AsObject();
                EntryNode->CreateUserDefinedPin(FName(*GetJsonStringField(Pin, TEXT("name")).TrimStartAndEnd()),
                    McpBlueprintUtils::MakePinType(GetJsonStringField(Pin, TEXT("type"))), EGPD_Output);
            }
            break;
        }
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    const bool bCompiled = McpSafeCompileBlueprint(Blueprint);
    const bool bSaved = McpSafeAssetSave(Blueprint);
    // An RPC only travels when its actor (or component) replicates; say so, never flip it.
    const UObject* Defaults = Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
    const AActor* ActorDefaults = Cast<AActor>(Defaults);
    const UActorComponent* ComponentDefaults = Cast<UActorComponent>(Defaults);
    const bool bReplicates = ActorDefaults ? ActorDefaults->GetIsReplicated() : (ComponentDefaults && ComponentDefaults->GetIsReplicated());

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("functionName"), FunctionName);
    ResultJson->SetStringField(TEXT("rpcType"), RpcName);
    ResultJson->SetBoolField(TEXT("reliable"), bReliable);
    ResultJson->SetBoolField(TEXT("compiled"), bCompiled);
    ResultJson->SetBoolField(TEXT("saved"), bSaved);
    ResultJson->SetBoolField(TEXT("actorReplicates"), bReplicates);
    ResultJson->SetStringField(TEXT("message"), bReplicates
        ? FString::Printf(TEXT("Created %s RPC %s with %d input(s); its body is the function graph of that name."), *RpcName, *FunctionName, Inputs.Num())
        : FString::Printf(TEXT("Created %s RPC %s with %d input(s), but %s does not replicate, so the call never leaves the machine that makes it until replication is turned on."), *RpcName, *FunctionName, Inputs.Num(), *Blueprint->GetName()));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("RPC function created"), ResultJson);
    return true;
}
}

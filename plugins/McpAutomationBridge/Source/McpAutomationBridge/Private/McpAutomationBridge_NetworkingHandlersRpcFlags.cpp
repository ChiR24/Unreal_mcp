#include "McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
static bool FindFunctionEntryNode(
    UBlueprint* Blueprint,
    const FString& FunctionName,
    UK2Node_FunctionEntry*& OutEntryNode)
{
    OutEntryNode = nullptr;
    for (UEdGraph* Graph : Blueprint->FunctionGraphs)
    {
        if (!Graph || Graph->GetFName() != FName(*FunctionName))
        {
            continue;
        }

        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (UK2Node_FunctionEntry* EntryNode = Cast<UK2Node_FunctionEntry>(Node))
            {
                OutEntryNode = EntryNode;
                return true;
            }
        }
        return false;
    }
    return false;
}

static bool LoadRpcBlueprintAndEntry(
    FNetworkingActionContext& Context,
    UBlueprint*& OutBlueprint,
    UK2Node_FunctionEntry*& OutEntryNode,
    FString& OutFunctionName)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
    OutFunctionName = GetStringField(Payload, TEXT("functionName"));

    if (BlueprintPath.IsEmpty() || OutFunctionName.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
        return false;
    }

    OutBlueprint = LoadBlueprintFromPath(BlueprintPath);
    if (!OutBlueprint)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
        return false;
    }

    if (!FindFunctionEntryNode(OutBlueprint, OutFunctionName, OutEntryNode))
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, FString::Printf(TEXT("Function '%s' not found"), *OutFunctionName), TEXT("NOT_FOUND"));
        return false;
    }

    if (!OutEntryNode)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Function entry node not found"), TEXT("NOT_FOUND"));
        return false;
    }
    return true;
}

bool HandleConfigureRpcValidation(FNetworkingActionContext& Context)
{
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    UBlueprint* Blueprint = nullptr;
    UK2Node_FunctionEntry* EntryNode = nullptr;
    FString FunctionName;
    if (!LoadRpcBlueprintAndEntry(Context, Blueprint, EntryNode, FunctionName))
    {
        return true;
    }

    bool bWithValidation = GetBoolField(Context.Payload, TEXT("withValidation"), true);
    if (bWithValidation)
    {
        EntryNode->AddExtraFlags(FUNC_NetValidate);
    }
    else
    {
        EntryNode->ClearExtraFlags(FUNC_NetValidate);
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    McpSafeAssetSave(Blueprint);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetBoolField(TEXT("withValidation"), bWithValidation);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("RPC validation %s for function %s"), bWithValidation ? TEXT("enabled") : TEXT("disabled"), *FunctionName));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("RPC validation configured"), ResultJson);
    return true;
}

bool HandleSetRpcReliability(FNetworkingActionContext& Context)
{
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    UBlueprint* Blueprint = nullptr;
    UK2Node_FunctionEntry* EntryNode = nullptr;
    FString FunctionName;
    if (!LoadRpcBlueprintAndEntry(Context, Blueprint, EntryNode, FunctionName))
    {
        return true;
    }

    bool bReliable = GetBoolField(Context.Payload, TEXT("reliable"), true);
    if (bReliable)
    {
        EntryNode->AddExtraFlags(FUNC_NetReliable);
    }
    else
    {
        EntryNode->ClearExtraFlags(FUNC_NetReliable);
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    McpSafeAssetSave(Blueprint);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetBoolField(TEXT("reliable"), bReliable);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("RPC %s reliability set to %s"), *FunctionName, bReliable ? TEXT("reliable") : TEXT("unreliable")));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("RPC reliability configured"), ResultJson);
    return true;
}
}

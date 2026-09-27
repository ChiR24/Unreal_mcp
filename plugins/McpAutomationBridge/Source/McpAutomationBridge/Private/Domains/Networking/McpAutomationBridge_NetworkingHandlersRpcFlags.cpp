#include "Domains/Networking/McpAutomationBridge_NetworkingHandlersPrivate.h"

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
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    OutFunctionName = GetJsonStringField(Payload, TEXT("functionName"));

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

    return true;
}

// Sets or clears Flag on the payload's RPC per its bool Field (default true), then recompiles, saves and replies.
static bool SetRpcFlag(FNetworkingActionContext& Context, EFunctionFlags Flag, const TCHAR* Field, const TCHAR* What)
{
    UBlueprint* Blueprint = nullptr;
    UK2Node_FunctionEntry* EntryNode = nullptr;
    FString FunctionName;
    if (!LoadRpcBlueprintAndEntry(Context, Blueprint, EntryNode, FunctionName))
    {
        return true;
    }
    const bool bOn = GetJsonBoolField(Context.Payload, Field, true);
    if (bOn)
    {
        EntryNode->AddExtraFlags(Flag);
    }
    else
    {
        EntryNode->ClearExtraFlags(Flag);
    }
    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    McpSafeAssetSave(Blueprint);

    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetBoolField(Field, bOn);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("RPC %s %s for function %s"), What, bOn ? TEXT("enabled") : TEXT("disabled"), *FunctionName));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true,
        FString::Printf(TEXT("RPC %s configured"), What), ResultJson);
    return true;
}

bool HandleConfigureRpcValidation(FNetworkingActionContext& Context)
{
    return SetRpcFlag(Context, FUNC_NetValidate, TEXT("withValidation"), TEXT("validation"));
}

bool HandleSetRpcReliability(FNetworkingActionContext& Context)
{
    return SetRpcFlag(Context, FUNC_NetReliable, TEXT("reliable"), TEXT("reliability"));
}
}

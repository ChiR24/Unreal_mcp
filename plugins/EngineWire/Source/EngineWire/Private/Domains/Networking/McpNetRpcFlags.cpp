#include "Domains/Networking/McpNetPrivate.h"

#include "K2Node_CustomEvent.h"

namespace McpNetworkingHandlers
{
namespace
{
// An RPC is a function graph whose entry carries the net flags (what create_rpc_function makes) or a custom event
// set to replicate in the editor (its own FunctionFlags). Only function graphs were searched, so the editor's
// usual RPC, a replicated custom event, answered "not found".
struct FRpcNode
{
    UK2Node_FunctionEntry* Entry = nullptr;
    UK2Node_CustomEvent* Event = nullptr;

    UEdGraphNode* Node() const { return Entry ? static_cast<UEdGraphNode*>(Entry) : Event; }
    uint32 Flags() const { return Entry ? static_cast<uint32>(Entry->GetExtraFlags()) : Event->FunctionFlags; }
    void Set(uint32 Flag, bool bOn)
    {
        if (Entry)
        {
            bOn ? Entry->AddExtraFlags(Flag) : Entry->ClearExtraFlags(Flag);
        }
        else
        {
            Event->FunctionFlags = bOn ? (Event->FunctionFlags | Flag) : (Event->FunctionFlags & ~Flag);
        }
    }
};

FRpcNode FindRpcNode(UBlueprint* Blueprint, const FName Name)
{
    FRpcNode Found;
    if (UEdGraph* Graph = FindFunctionGraph(Blueprint, Name))
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if ((Found.Entry = Cast<UK2Node_FunctionEntry>(Node)) != nullptr)
            {
                return Found;
            }
        }
    }
    for (UEdGraph* Graph : Blueprint->UbergraphPages)
    {
        if (!Graph)
        {
            continue;
        }
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            UK2Node_CustomEvent* Event = Cast<UK2Node_CustomEvent>(Node);
            if (Event && Event->CustomFunctionName == Name)
            {
                Found.Event = Event;
                return Found;
            }
        }
    }
    return Found;
}

// Sets or clears Flag on the payload's RPC per its bool Field (default true). A flag that already has the asked
// value changes nothing (no compile, no save); otherwise the Blueprint recompiles and saves.
bool SetRpcFlag(FNetworkingActionContext& Context, EFunctionFlags Flag, const TCHAR* Field, const TCHAR* What)
{
    const FString FunctionName = GetJsonStringField(Context.Payload, TEXT("functionName")).TrimStartAndEnd();
    if (FunctionName.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing functionName"), TEXT("INVALID_PARAMS"));
        return true;
    }
    UBlueprint* Blueprint = LoadBlueprintOrReply(Context, GetJsonStringField(Context.Payload, TEXT("blueprintPath")));
    if (!Blueprint)
    {
        return true;
    }
    FRpcNode Rpc = FindRpcNode(Blueprint, FName(*FunctionName));
    if (!Rpc.Node())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            FString::Printf(TEXT("%s has no function or custom event named '%s'"), *Blueprint->GetName(), *FunctionName), TEXT("NOT_FOUND"));
        return true;
    }
    // Reliability on a plain function or event was stored, compiled and reported, and did nothing.
    if ((Rpc.Flags() & FUNC_Net) == 0)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            FString::Printf(TEXT("'%s' is not an RPC (it does not replicate to the server, a client or everyone). Create one with configure_rpc setting=create. Nothing was changed."), *FunctionName),
            TEXT("NOT_AN_RPC"));
        return true;
    }
    const bool bOn = GetJsonBoolField(Context.Payload, Field, true);
    const bool bChanged = ((Rpc.Flags() & Flag) != 0) != bOn;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    if (bChanged)
    {
        Blueprint->Modify();
        Rpc.Node()->Modify();
        Rpc.Set(Flag, bOn);
        // As the editor's Reliable checkbox does: the function's signature flags changed.
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        ResultJson->SetBoolField(TEXT("compiled"), McpSafeCompileBlueprint(Blueprint));
        ResultJson->SetBoolField(TEXT("saved"), McpSafeAssetSave(Blueprint));
    }
    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("functionName"), FunctionName);
    ResultJson->SetBoolField(Field, bOn);
    ResultJson->SetBoolField(TEXT("changed"), bChanged);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("RPC %s %s for %s%s"), What, bOn ? TEXT("on") : TEXT("off"), *FunctionName,
        bChanged ? TEXT("") : TEXT(" (already so; nothing changed)")));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    if (!bChanged)
    {
        McpHandlerUtils::MarkNoAssetsChanged(ResultJson);
    }
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true,
        FString::Printf(TEXT("RPC %s configured"), What), ResultJson);
    return true;
}
}

bool HandleConfigureRpcValidation(FNetworkingActionContext& Context)
{
    // FUNC_NetValidate is read only by the thunk UHT generates for a C++ RPC (it calls <Name>_Validate and drops the
    // caller's connection on false). A Blueprint function or event has no thunk, so setting the flag answered
    // "validation enabled" for a check that never ran. withValidation false still clears a flag left that way.
    if (GetJsonBoolField(Context.Payload, TEXT("withValidation"), true))
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            TEXT("Blueprint RPCs have no validation step: WithValidation exists only for C++ RPCs. Check the parameters at the start of the RPC's body instead, with a Branch whose false path ends the call (manage_blueprint edit_graph). Nothing was changed."),
            TEXT("NOT_SUPPORTED"));
        return true;
    }
    return SetRpcFlag(Context, FUNC_NetValidate, TEXT("withValidation"), TEXT("validation"));
}

bool HandleSetRpcReliability(FNetworkingActionContext& Context)
{
    return SetRpcFlag(Context, FUNC_NetReliable, TEXT("reliable"), TEXT("reliability"));
}
}

#include "Domains/Networking/McpNetPrivate.h"

namespace McpNetworkingHandlers
{
namespace
{
// A RepNotify function must take no parameters and return nothing: the compiler clears RepNotifyFunc on a
// variable whose function is missing or has either (BlueprintClassFlagUtils), and the notify never fires.
bool HasParametersOrResult(UEdGraph* Graph)
{
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        const UK2Node_EditablePinBase* Terminator = Cast<UK2Node_EditablePinBase>(Node);
        if (Terminator && Terminator->UserDefinedPins.Num() > 0)
        {
            return true;
        }
    }
    return false;
}
}

bool HandleSetReplicatedUsing(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    const FString PropertyName = GetJsonStringField(Payload, TEXT("propertyName"));
    const FString RepNotifyFunc = GetJsonStringField(Payload, TEXT("repNotifyFunc")).TrimStartAndEnd();

    if (BlueprintPath.IsEmpty() || PropertyName.IsEmpty() || RepNotifyFunc.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("blueprintPath, propertyName and repNotifyFunc are required"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
    if (!Blueprint)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
        return true;
    }
    FBPVariableDescription* Variable = Blueprint->NewVariables.FindByPredicate(
        [&PropertyName](const FBPVariableDescription& Each) { return Each.VarName == FName(*PropertyName); });
    if (!Variable)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, FString::Printf(TEXT("Property '%s' not found"), *PropertyName), TEXT("NOT_FOUND"));
        return true;
    }

    // The function the variable names used to be stored unchecked; a missing one compiled away silently. It is
    // the Blueprint's own, an inherited one, or created empty here as the editor's RepNotify setting does.
    const FName FuncName(*RepNotifyFunc);
    UEdGraph* FuncGraph = FindFunctionGraph(Blueprint, FuncName);
    const UFunction* Inherited = FuncGraph || !Blueprint->ParentClass ? nullptr : Blueprint->ParentClass->FindFunctionByName(FuncName);
    if ((FuncGraph && HasParametersOrResult(FuncGraph)) || (Inherited && (Inherited->NumParms > 0 || Inherited->GetReturnProperty())))
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            FString::Printf(TEXT("'%s' takes parameters or returns a value; a RepNotify function takes none and returns nothing (the compiler drops any other). Name a different function. Nothing was changed."), *RepNotifyFunc),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }
    const bool bCreate = !FuncGraph && !Inherited;
    if (bCreate && !CheckFreeMemberNameOrReply(Context, Blueprint, RepNotifyFunc, TEXT("repNotifyFunc")))
    {
        return true;
    }

    Blueprint->Modify();
    if (bCreate)
    {
        FuncGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FuncName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
        FBlueprintEditorUtils::AddFunctionGraph<UClass>(Blueprint, FuncGraph, false, nullptr);
    }
    Variable->PropertyFlags |= CPF_Net | CPF_RepNotify;
    Variable->RepNotifyFunc = FuncName;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    ResultJson->SetBoolField(TEXT("compiled"), McpSafeCompileBlueprint(Blueprint));
    ResultJson->SetBoolField(TEXT("saved"), McpSafeAssetSave(Blueprint));

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("repNotifyFunc"), RepNotifyFunc);
    ResultJson->SetBoolField(TEXT("functionCreated"), bCreate);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("%s replicates and calls %s on clients when a new value arrives%s"), *PropertyName, *RepNotifyFunc,
        bCreate ? TEXT("; the function was created empty, so give it a body") : TEXT("")));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("ReplicatedUsing configured"), ResultJson);
    return true;
}

bool HandleConfigurePushModel(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    bool bUsePushModel = GetJsonBoolField(Payload, TEXT("usePushModel"), true);

    UBlueprint* Blueprint = LoadBlueprintOrReply(Context, BlueprintPath);
    if (!Blueprint)
    {
        return true;
    }

    int32 ReplicatedCount = 0;
    for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
    {
        if ((VarDesc.PropertyFlags & CPF_Net) != 0)
        {
            bUsePushModel ? VarDesc.SetMetaData(TEXT("PushModel"), TEXT("true")) : VarDesc.RemoveMetaData(TEXT("PushModel"));
            ++ReplicatedCount;
        }
    }
    // With no replicated variable there is nothing push model could apply to; the old reply
    // claimed it was enabled "for all replicated properties" anyway.
    if (ReplicatedCount == 0)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            TEXT("The Blueprint has no replicated variables for push model to apply to; replicate one first (set_property_replicated)."),
            TEXT("NOT_FOUND"));
        return true;
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    McpSafeAssetSave(Blueprint);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetBoolField(TEXT("usePushModel"), bUsePushModel);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Push model replication %s for %d replicated variable(s)"), bUsePushModel ? TEXT("enabled") : TEXT("disabled"), ReplicatedCount));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Push model configured"), ResultJson);
    return true;
}
}

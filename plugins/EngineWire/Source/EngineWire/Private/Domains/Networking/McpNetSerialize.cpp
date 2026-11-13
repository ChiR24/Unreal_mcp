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

    UBlueprint* Blueprint = LoadBlueprintOrReply(Context, BlueprintPath);
    if (!Blueprint)
    {
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
    ResultJson->SetStringField(TEXT("repNotifyFunc"), RepNotifyFunc);
    ResultJson->SetBoolField(TEXT("functionCreated"), bCreate);
    return SaveBlueprintAndReply(Context, Blueprint,
        FString::Printf(TEXT("%s replicates and calls %s on clients when a new value arrives%s"), *PropertyName, *RepNotifyFunc,
                        bCreate ? TEXT("; the function was created empty, so give it a body") : TEXT("")),
        TEXT("ReplicatedUsing configured"), true);
}

bool HandleConfigurePushModel(FNetworkingActionContext& Context)
{
    // No engine code reads a variable's PushModel entry (PushModel.cpp, 5.0 to 5.8): turning it on answered "enabled"
    // for a switch nothing reads. usePushModel false still clears the entries an earlier call left.
    if (GetJsonBoolField(Context.Payload, TEXT("usePushModel"), true))
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            TEXT("Push model is not set per variable: no engine code reads a variable's PushModel entry. Whether replicated Blueprint variables use it is set for the whole project by the console variables Net.IsPushModelEnabled (off by default) and Net.MakeBpPropertiesPushModel (on by default, so Blueprint variables follow once push model is on); set them in the project's config. Nothing was changed."),
            TEXT("NOT_SUPPORTED"));
        return true;
    }
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

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
            VarDesc.RemoveMetaData(TEXT("PushModel"));
            ++ReplicatedCount;
        }
    }
    // With no replicated variable there is no entry to clear.
    if (ReplicatedCount == 0)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            TEXT("The Blueprint has no replicated variables, so no PushModel entry to clear."),
            TEXT("NOT_FOUND"));
        return true;
    }

    ResultJson->SetBoolField(TEXT("usePushModel"), false);
    return SaveBlueprintAndReply(Context, Blueprint,
        FString::Printf(TEXT("PushModel entries cleared from %d replicated variable(s)"), ReplicatedCount), TEXT("Push model configured"), true);
}
}

#include "Domains/Networking/McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
bool HandleSetReplicatedUsing(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString PropertyName = GetJsonStringField(Payload, TEXT("propertyName"));
    FString RepNotifyFunc = GetJsonStringField(Payload, TEXT("repNotifyFunc"));

    if (BlueprintPath.IsEmpty() || PropertyName.IsEmpty() || RepNotifyFunc.IsEmpty())
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

    bool bFound = false;
    for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
    {
        if (VarDesc.VarName == FName(*PropertyName))
        {
            VarDesc.PropertyFlags |= CPF_Net | CPF_RepNotify;
            VarDesc.RepNotifyFunc = FName(*RepNotifyFunc);
            bFound = true;
            break;
        }
    }

    if (!bFound)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, FString::Printf(TEXT("Property '%s' not found"), *PropertyName), TEXT("NOT_FOUND"));
        return true;
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint);
    McpSafeAssetSave(Blueprint);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("ReplicatedUsing set to %s for property %s"), *RepNotifyFunc, *PropertyName));
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

#include "Domains/Networking/McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
bool HandleConfigureNetSerialization(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString StructName = GetJsonStringField(Payload, TEXT("structName"));
    bool bCustomSerialization = GetJsonBoolField(Payload, TEXT("customSerialization"), false);

    if (BlueprintPath.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
    if (!Blueprint)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
        return true;
    }

    // Nothing here is settable from a Blueprint CDO: custom NetSerialize lives
    // on a USTRUCT in C++, and bReplicateUsingRegisteredSubObjectList is
    // protected. The handler used to log that, dirty and SAVE the blueprint
    // anyway, and answer "Net serialization configured" -- a write receipt for
    // a call that changed nothing and still bumped the asset's revision.
    // Refuse instead, and leave the asset untouched.
    Context.Bridge.SendAutomationResponse(
        Context.RequestingSocket, Context.RequestId, false,
        FString::Printf(
            TEXT("Net serialization cannot be configured from a Blueprint%s: custom NetSerialize is declared on a C++ USTRUCT, and bReplicateUsingRegisteredSubObjectList is protected. Nothing was changed on '%s'."),
            StructName.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" for struct '%s'"), *StructName),
            *BlueprintPath),
        ResultJson, TEXT("NOT_SUPPORTED"));
    (void)bCustomSerialization;
    return true;
}

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

    if (BlueprintPath.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
    if (!Blueprint)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
        return true;
    }

    bool bAnyModified = false;
    for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
    {
        if ((VarDesc.PropertyFlags & CPF_Net) != 0)
        {
            bUsePushModel ? VarDesc.SetMetaData(TEXT("PushModel"), TEXT("true")) : VarDesc.RemoveMetaData(TEXT("PushModel"));
            bAnyModified = true;
        }
    }

    if (bAnyModified)
    {
        Blueprint->Modify();
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);
        McpSafeAssetSave(Blueprint);
    }

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetBoolField(TEXT("usePushModel"), bUsePushModel);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Push model replication %s for all replicated properties"), bUsePushModel ? TEXT("enabled") : TEXT("disabled")));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Push model configured"), ResultJson);
    return true;
}
}

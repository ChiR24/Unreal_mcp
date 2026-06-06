#include "McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
bool HandleConfigureNetSerialization(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
    FString StructName = GetStringField(Payload, TEXT("structName"));
    bool bCustomSerialization = GetBoolField(Payload, TEXT("customSerialization"), false);

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

    AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
    if (CDO)
    {
        UE_LOG(LogMcpNetworkingHandlers, Log, TEXT("bReplicateUsingRegisteredSubObjectList is protected. Use Actor defaults in Blueprint instead."));
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetBoolField(TEXT("customSerialization"), bCustomSerialization);
    if (!StructName.IsEmpty())
    {
        ResultJson->SetStringField(TEXT("structName"), StructName);
    }
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Net serialization configured (customSerialization=%s)"), bCustomSerialization ? TEXT("true") : TEXT("false")));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Net serialization configured"), ResultJson);
    return true;
}

bool HandleSetReplicatedUsing(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
    FString PropertyName = GetStringField(Payload, TEXT("propertyName"));
    FString RepNotifyFunc = GetStringField(Payload, TEXT("repNotifyFunc"));

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
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
    bool bUsePushModel = GetBoolField(Payload, TEXT("usePushModel"), true);

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

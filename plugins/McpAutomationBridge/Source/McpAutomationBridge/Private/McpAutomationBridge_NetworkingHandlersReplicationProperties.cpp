#include "McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
bool HandleSetPropertyReplicated(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
    FString PropertyName = GetStringField(Payload, TEXT("propertyName"));
    bool bReplicated = GetBoolField(Payload, TEXT("replicated"), true);

    if (BlueprintPath.IsEmpty() || PropertyName.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing blueprintPath or propertyName"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
    if (!Blueprint)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
        return true;
    }

    FProperty* Property = nullptr;
    for (TFieldIterator<FProperty> It(Blueprint->GeneratedClass); It; ++It)
    {
        if (It->GetName() == PropertyName)
        {
            Property = *It;
            break;
        }
    }

    if (!Property)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Property not found in blueprint"), TEXT("NOT_FOUND"));
        return true;
    }

    if (bReplicated)
    {
        Property->SetPropertyFlags(CPF_Net);
    }
    else
    {
        Property->ClearPropertyFlags(CPF_Net);
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Property %s replication set to %s"), *PropertyName, bReplicated ? TEXT("true") : TEXT("false")));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Property replication configured"), ResultJson);
    return true;
}

bool HandleSetReplicationCondition(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
    FString PropertyName = GetStringField(Payload, TEXT("propertyName"));
    FString Condition = GetStringField(Payload, TEXT("condition"));

    if (BlueprintPath.IsEmpty() || PropertyName.IsEmpty() || Condition.IsEmpty())
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

    ELifetimeCondition LifetimeCondition = GetReplicationCondition(Condition);
    bool bFound = false;
    for (FBPVariableDescription& VarDesc : Blueprint->NewVariables)
    {
        if (VarDesc.VarName == FName(*PropertyName))
        {
            VarDesc.PropertyFlags |= CPF_Net;
            VarDesc.ReplicationCondition = LifetimeCondition;
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
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Replication condition set to %s"), *Condition));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Replication condition configured"), ResultJson);
    return true;
}

bool HandleConfigureNetUpdateFrequency(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
    double NetUpdateFrequency = GetNumberField(Payload, TEXT("netUpdateFrequency"), 100.0);
    double MinNetUpdateFrequency = GetNumberField(Payload, TEXT("minNetUpdateFrequency"), 2.0);

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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    if (CDO)
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
        CDO->SetNetUpdateFrequency(static_cast<float>(NetUpdateFrequency));
        CDO->SetMinNetUpdateFrequency(static_cast<float>(MinNetUpdateFrequency));
#else
        CDO->NetUpdateFrequency = static_cast<float>(NetUpdateFrequency);
        CDO->MinNetUpdateFrequency = static_cast<float>(MinNetUpdateFrequency);
#endif
    }
#else
    Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Net update frequency APIs not available in UE 5.0"), TEXT("NOT_AVAILABLE"));
    return true;
#endif

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Net update frequency set to %.1f (min: %.1f)"), NetUpdateFrequency, MinNetUpdateFrequency));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Net update frequency configured"), ResultJson);
    return true;
}
}

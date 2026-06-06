#include "McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
bool HandleConfigureNetPriority(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
    double NetPriority = GetNumberField(Payload, TEXT("netPriority"), 1.0);

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
        CDO->NetPriority = static_cast<float>(NetPriority);
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Net priority set to %.2f"), NetPriority));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Net priority configured"), ResultJson);
    return true;
}

bool HandleSetNetDormancy(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
    FString Dormancy = GetStringField(Payload, TEXT("dormancy"));

    if (BlueprintPath.IsEmpty() || Dormancy.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing blueprintPath or dormancy"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
    if (!Blueprint)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
        return true;
    }

    ENetDormancy NetDormancy = GetNetDormancy(Dormancy);
    AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
    if (CDO)
    {
        CDO->NetDormancy = NetDormancy;
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Net dormancy set to %s"), *Dormancy));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Net dormancy configured"), ResultJson);
    return true;
}

bool HandleConfigureReplicationGraph(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetStringField(Payload, TEXT("blueprintPath"));
    bool bSpatiallyLoaded = GetBoolField(Payload, TEXT("spatiallyLoaded"), false);
    bool bNetLoadOnClient = GetBoolField(Payload, TEXT("netLoadOnClient"), true);
    FString ReplicationPolicy = GetStringField(Payload, TEXT("replicationPolicy"), TEXT("Default"));

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
        CDO->bNetLoadOnClient = bNetLoadOnClient;
        if (bSpatiallyLoaded)
        {
            UE_LOG(LogMcpNetworkingHandlers, Log, TEXT("bReplicateUsingRegisteredSubObjectList is protected. Use Actor defaults in Blueprint instead."));
        }
    }

    Blueprint->Modify();
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeAssetSave(Blueprint);

    ResultJson->SetBoolField(TEXT("success"), true);
    ResultJson->SetBoolField(TEXT("spatiallyLoaded"), bSpatiallyLoaded);
    ResultJson->SetBoolField(TEXT("netLoadOnClient"), bNetLoadOnClient);
    ResultJson->SetStringField(TEXT("replicationPolicy"), ReplicationPolicy);
    ResultJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Replication graph settings configured (netLoadOnClient=%s, spatiallyLoaded=%s)"),
        bNetLoadOnClient ? TEXT("true") : TEXT("false"),
        bSpatiallyLoaded ? TEXT("true") : TEXT("false")));
    McpHandlerUtils::AddVerification(ResultJson, Blueprint);
    Context.Bridge.SendAutomationResponse(Context.RequestingSocket, Context.RequestId, true, TEXT("Replication graph configured"), ResultJson);
    return true;
}
}

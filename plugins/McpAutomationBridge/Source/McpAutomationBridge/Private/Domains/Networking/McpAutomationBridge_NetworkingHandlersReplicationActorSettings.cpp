#include "Domains/Networking/McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
bool HandleConfigureNetPriority(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    double NetPriority = GetJsonNumberField(Payload, TEXT("netPriority"), 1.0);

    UBlueprint* Blueprint = LoadBlueprintOrReply(Context, BlueprintPath);
    if (!Blueprint)
    {
        return true;
    }

    AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject());
    if (CDO)
    {
        CDO->NetPriority = static_cast<float>(NetPriority);
    }

    return SaveBlueprintAndReply(Context, Blueprint, FString::Printf(TEXT("Net priority set to %.2f"), NetPriority), TEXT("Net priority configured"));
}

bool HandleSetNetDormancy(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString Dormancy = GetJsonStringField(Payload, TEXT("dormancy"));

    if (BlueprintPath.IsEmpty() || Dormancy.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing blueprintPath or dormancy"), TEXT("INVALID_PARAMS"));
        return true;
    }

    ENetDormancy NetDormancy = DORM_Never;
    FString ValidDormancies;
    if (!TryParseNetEnum(Dormancy, NetDormancy, ValidDormancies))
    {
        ReplyInvalidEnum(Context, TEXT("dormancy"), Dormancy, ValidDormancies);
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintFromPath(BlueprintPath);
    if (!Blueprint)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found"), TEXT("NOT_FOUND"));
        return true;
    }

    AActor* CDO = Blueprint->GeneratedClass ? Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!CDO)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Net dormancy is an Actor setting; this Blueprint is not an Actor."), TEXT("NOT_SUPPORTED"));
        return true;
    }
    CDO->NetDormancy = NetDormancy;

    return SaveBlueprintAndReply(Context, Blueprint, FString::Printf(TEXT("Net dormancy set to %s"), *NetDormancyToString(NetDormancy)), TEXT("Net dormancy configured"));
}

bool HandleConfigureReplicationGraph(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    // Only the fields that were sent are written: netLoadOnClient used to be forced to true whenever
    // it was left out, and spatiallyLoaded was only logged.
    const bool bHasSpatiallyLoaded = Payload->HasField(TEXT("spatiallyLoaded"));
    const bool bHasNetLoadOnClient = Payload->HasField(TEXT("netLoadOnClient"));
    if (!bHasSpatiallyLoaded && !bHasNetLoadOnClient)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Nothing to configure: pass spatiallyLoaded, netLoadOnClient, or both."), TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintOrReply(Context, BlueprintPath);
    if (!Blueprint)
    {
        return true;
    }

    AActor* CDO = Blueprint->GeneratedClass ? Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!CDO)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("These are Actor settings; this Blueprint is not an Actor."), TEXT("NOT_SUPPORTED"));
        return true;
    }
    if (bHasSpatiallyLoaded)
    {
        const bool bSpatiallyLoaded = GetJsonBoolField(Payload, TEXT("spatiallyLoaded"), false);
        if (CDO->GetIsSpatiallyLoaded() != bSpatiallyLoaded && !CDO->CanChangeIsSpatiallyLoadedFlag())
        {
            Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, FString::Printf(TEXT("%s does not allow changing its spatially loaded flag."), *CDO->GetClass()->GetName()), TEXT("NOT_SUPPORTED"));
            return true;
        }
        CDO->SetIsSpatiallyLoaded(bSpatiallyLoaded);
    }
    if (bHasNetLoadOnClient)
    {
        CDO->bNetLoadOnClient = GetJsonBoolField(Payload, TEXT("netLoadOnClient"), true);
    }

    ResultJson->SetBoolField(TEXT("spatiallyLoaded"), CDO->GetIsSpatiallyLoaded());
    ResultJson->SetBoolField(TEXT("netLoadOnClient"), CDO->bNetLoadOnClient);
    return SaveBlueprintAndReply(Context, Blueprint,
        FString::Printf(TEXT("netLoadOnClient=%s, spatiallyLoaded=%s"),
            CDO->bNetLoadOnClient ? TEXT("true") : TEXT("false"), CDO->GetIsSpatiallyLoaded() ? TEXT("true") : TEXT("false")),
        TEXT("Replication graph configured"));
}
}

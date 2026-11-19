#include "Domains/Networking/McpNetPrivate.h"

namespace McpNetworkingHandlers
{
bool HandleConfigureNetCullDistance(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    // Only the fields sent are written: naming only useOwnerNetRelevancy set the distance to 225000000, and naming only
    // the distance turned owner relevancy off.
    double NetCullDistanceSquared = 0.0;
    bool bUseOwnerNetRelevancy = false;
    const bool bHasDistance = Payload->TryGetNumberField(TEXT("netCullDistanceSquared"), NetCullDistanceSquared);
    const bool bHasOwner = Payload->TryGetBoolField(TEXT("useOwnerNetRelevancy"), bUseOwnerNetRelevancy);
    if (!bHasDistance && !bHasOwner)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Nothing to configure: pass netCullDistanceSquared, useOwnerNetRelevancy, or both."), TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = nullptr;
    AActor* CDO = LoadActorDefaultsOrReply(Context, BlueprintPath, Blueprint);
    if (!CDO)
    {
        return true;
    }
    if (bHasDistance)
    {
#if MCP_HAS_ACTOR_NET_CULL_DISTANCE_ACCESSORS
        CDO->SetNetCullDistanceSquared(static_cast<float>(NetCullDistanceSquared));
#else
        CDO->NetCullDistanceSquared = static_cast<float>(NetCullDistanceSquared);
#endif
    }
    if (bHasOwner)
    {
        CDO->bNetUseOwnerRelevancy = bUseOwnerNetRelevancy;
    }
#if MCP_HAS_ACTOR_NET_CULL_DISTANCE_ACCESSORS
    const float StoredDistance = CDO->GetNetCullDistanceSquared();
#else
    const float StoredDistance = CDO->NetCullDistanceSquared;
#endif

    return SaveBlueprintAndReply(Context, Blueprint, FString::Printf(TEXT("Net cull distance squared %.0f, owner relevancy %s"),
        StoredDistance, CDO->bNetUseOwnerRelevancy ? TEXT("on") : TEXT("off")), TEXT("Net cull distance configured"));
}

bool HandleSetAlwaysRelevant(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    bool bAlwaysRelevant = GetJsonBoolField(Payload, TEXT("alwaysRelevant"), true);

    UBlueprint* Blueprint = nullptr;
    AActor* CDO = LoadActorDefaultsOrReply(Context, BlueprintPath, Blueprint);
    if (!CDO)
    {
        return true;
    }
    CDO->bAlwaysRelevant = bAlwaysRelevant;

    return SaveBlueprintAndReply(Context, Blueprint, FString::Printf(TEXT("Always relevant set to %s"), bAlwaysRelevant ? TEXT("true") : TEXT("false")), TEXT("Always relevant configured"));
}

bool HandleSetOnlyRelevantToOwner(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    bool bOnlyRelevantToOwner = GetJsonBoolField(Payload, TEXT("onlyRelevantToOwner"), true);

    UBlueprint* Blueprint = nullptr;
    AActor* CDO = LoadActorDefaultsOrReply(Context, BlueprintPath, Blueprint);
    if (!CDO)
    {
        return true;
    }
    CDO->bOnlyRelevantToOwner = bOnlyRelevantToOwner;

    return SaveBlueprintAndReply(Context, Blueprint, FString::Printf(TEXT("Only relevant to owner set to %s"), bOnlyRelevantToOwner ? TEXT("true") : TEXT("false")), TEXT("Only relevant to owner configured"));
}
}

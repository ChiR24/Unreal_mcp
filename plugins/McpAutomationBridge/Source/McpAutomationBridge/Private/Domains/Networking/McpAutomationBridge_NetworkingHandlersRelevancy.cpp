#include "Domains/Networking/McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
bool HandleConfigureNetCullDistance(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    double NetCullDistanceSquared = GetJsonNumberField(Payload, TEXT("netCullDistanceSquared"), 225000000.0);
    bool bUseOwnerNetRelevancy = GetJsonBoolField(Payload, TEXT("useOwnerNetRelevancy"), false);

    UBlueprint* Blueprint = LoadBlueprintOrReply(Context, BlueprintPath);
    if (!Blueprint)
    {
        return true;
    }

    // A Blueprint whose class never compiled has no defaults to write; the edit used to be skipped and reported.
    AActor* CDO = Blueprint->GeneratedClass ? Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!CDO)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("The Blueprint has no compiled Actor class to configure; compile it first."), TEXT("NOT_SUPPORTED"));
        return true;
    }
    if (CDO)
    {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5
        CDO->SetNetCullDistanceSquared(static_cast<float>(NetCullDistanceSquared));
#else
        CDO->NetCullDistanceSquared = static_cast<float>(NetCullDistanceSquared);
#endif
        CDO->bNetUseOwnerRelevancy = bUseOwnerNetRelevancy;
    }

    return SaveBlueprintAndReply(Context, Blueprint, FString::Printf(TEXT("Net cull distance squared set to %.0f"), NetCullDistanceSquared), TEXT("Net cull distance configured"));
}

bool HandleSetAlwaysRelevant(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    bool bAlwaysRelevant = GetJsonBoolField(Payload, TEXT("alwaysRelevant"), true);

    UBlueprint* Blueprint = LoadBlueprintOrReply(Context, BlueprintPath);
    if (!Blueprint)
    {
        return true;
    }

    // A Blueprint whose class never compiled has no defaults to write; the edit used to be skipped and reported.
    AActor* CDO = Blueprint->GeneratedClass ? Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!CDO)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("The Blueprint has no compiled Actor class to configure; compile it first."), TEXT("NOT_SUPPORTED"));
        return true;
    }
    if (CDO)
    {
        CDO->bAlwaysRelevant = bAlwaysRelevant;
    }

    return SaveBlueprintAndReply(Context, Blueprint, FString::Printf(TEXT("Always relevant set to %s"), bAlwaysRelevant ? TEXT("true") : TEXT("false")), TEXT("Always relevant configured"));
}

bool HandleSetOnlyRelevantToOwner(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    bool bOnlyRelevantToOwner = GetJsonBoolField(Payload, TEXT("onlyRelevantToOwner"), true);

    UBlueprint* Blueprint = LoadBlueprintOrReply(Context, BlueprintPath);
    if (!Blueprint)
    {
        return true;
    }

    // A Blueprint whose class never compiled has no defaults to write; the edit used to be skipped and reported.
    AActor* CDO = Blueprint->GeneratedClass ? Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!CDO)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("The Blueprint has no compiled Actor class to configure; compile it first."), TEXT("NOT_SUPPORTED"));
        return true;
    }
    if (CDO)
    {
        CDO->bOnlyRelevantToOwner = bOnlyRelevantToOwner;
    }

    return SaveBlueprintAndReply(Context, Blueprint, FString::Printf(TEXT("Only relevant to owner set to %s"), bOnlyRelevantToOwner ? TEXT("true") : TEXT("false")), TEXT("Only relevant to owner configured"));
}
}

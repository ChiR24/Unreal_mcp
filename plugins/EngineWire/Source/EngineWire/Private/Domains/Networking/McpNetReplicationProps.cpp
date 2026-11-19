#include "Domains/Networking/McpNetPrivate.h"

namespace McpNetworkingHandlers
{
bool HandleSetPropertyReplicated(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString PropertyName = GetJsonStringField(Payload, TEXT("propertyName"));
    bool bReplicated = GetJsonBoolField(Payload, TEXT("replicated"), true);
    const FString Condition = GetJsonStringField(Payload, TEXT("condition"));

    if (BlueprintPath.IsEmpty() || PropertyName.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing blueprintPath or propertyName"), TEXT("INVALID_PARAMS"));
        return true;
    }
    // condition is the replication condition to replicate under; it has no meaning on a variable
    // that stops replicating, so that combination is refused rather than half applied.
    ELifetimeCondition LifetimeCondition = COND_None;
    FString ValidConditions;
    if (!Condition.IsEmpty() && !TryParseNetEnum(Condition, LifetimeCondition, ValidConditions))
    {
        ReplyInvalidEnum(Context, TEXT("condition"), Condition, ValidConditions);
        return true;
    }
    if (!Condition.IsEmpty() && !bReplicated)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("condition only applies with replicated: true."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintOrReply(Context, BlueprintPath);
    if (!Blueprint)
    {
        return true;
    }

    // Setting CPF_Net on the GeneratedClass FProperty does not survive: the
    // class is regenerated from NewVariables on the next compile, so the flag
    // was written, saved, reported as done, and then silently dropped. The
    // authoritative home is FBPVariableDescription::PropertyFlags -- which
    // set_replication_condition in this same domain already writes. A name that
    // is not a Blueprint variable cannot carry the flag here at all, so say so
    // rather than writing something that evaporates.
    const FName PropertyFName(*PropertyName);
    FBPVariableDescription* VarDesc = Blueprint->NewVariables.FindByPredicate(
        [PropertyFName](const FBPVariableDescription& Desc) { return Desc.VarName == PropertyFName; });
    if (!VarDesc)
    {
        const bool bExistsOnClass = Blueprint->GeneratedClass &&
            Blueprint->GeneratedClass->FindPropertyByName(PropertyFName) != nullptr;
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId,
            bExistsOnClass
                ? FString::Printf(TEXT("'%s' is an inherited or native property, not a Blueprint variable; its replication is declared in C++ and cannot be changed from here."), *PropertyName)
                : FString::Printf(TEXT("Blueprint variable '%s' not found."), *PropertyName),
            TEXT("NOT_FOUND"));
        return true;
    }

    if (bReplicated)
    {
        VarDesc->PropertyFlags |= CPF_Net;
        if (!Condition.IsEmpty())
        {
            VarDesc->ReplicationCondition = LifetimeCondition;
        }
    }
    else
    {
        VarDesc->PropertyFlags &= ~static_cast<uint64>(CPF_Net);
    }

    return SaveBlueprintAndReply(Context, Blueprint,
        FString::Printf(TEXT("Property %s replication set to %s"), *PropertyName, bReplicated ? TEXT("true") : TEXT("false")),
        TEXT("Property replication configured"), true);
}

bool HandleSetReplicationCondition(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    TSharedPtr<FJsonObject>& ResultJson = Context.ResultJson;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    FString PropertyName = GetJsonStringField(Payload, TEXT("propertyName"));
    FString Condition = GetJsonStringField(Payload, TEXT("condition"));

    if (BlueprintPath.IsEmpty() || PropertyName.IsEmpty() || Condition.IsEmpty())
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing required parameters"), TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = LoadBlueprintOrReply(Context, BlueprintPath);
    if (!Blueprint)
    {
        return true;
    }

    ELifetimeCondition LifetimeCondition = COND_None;
    FString ValidConditions;
    if (!TryParseNetEnum(Condition, LifetimeCondition, ValidConditions))
    {
        ReplyInvalidEnum(Context, TEXT("condition"), Condition, ValidConditions);
        return true;
    }
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

    return SaveBlueprintAndReply(Context, Blueprint, FString::Printf(TEXT("Replication condition set to %s"), *Condition),
        TEXT("Replication condition configured"), true);
}

bool HandleConfigureNetUpdateFrequency(FNetworkingActionContext& Context)
{
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    // Only the fields sent are written: the one left out was set to 100 or 2.
    double NetUpdateFrequency = 0.0;
    double MinNetUpdateFrequency = 0.0;
    const bool bHasFrequency = Payload->TryGetNumberField(TEXT("netUpdateFrequency"), NetUpdateFrequency);
    const bool bHasMin = Payload->TryGetNumberField(TEXT("minNetUpdateFrequency"), MinNetUpdateFrequency);
    if (!bHasFrequency && !bHasMin)
    {
        Context.Bridge.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Nothing to configure: pass netUpdateFrequency, minNetUpdateFrequency, or both."), TEXT("INVALID_PARAMS"));
        return true;
    }

    UBlueprint* Blueprint = nullptr;
    AActor* CDO = LoadActorDefaultsOrReply(Context, BlueprintPath, Blueprint);
    if (!CDO)
    {
        return true;
    }
#if MCP_HAS_ACTOR_NET_UPDATE_FREQUENCY_ACCESSORS
    if (bHasFrequency) { CDO->SetNetUpdateFrequency(static_cast<float>(NetUpdateFrequency)); }
    if (bHasMin) { CDO->SetMinNetUpdateFrequency(static_cast<float>(MinNetUpdateFrequency)); }
    const float StoredFrequency = CDO->GetNetUpdateFrequency();
    const float StoredMin = CDO->GetMinNetUpdateFrequency();
#else
    if (bHasFrequency) { CDO->NetUpdateFrequency = static_cast<float>(NetUpdateFrequency); }
    if (bHasMin) { CDO->MinNetUpdateFrequency = static_cast<float>(MinNetUpdateFrequency); }
    const float StoredFrequency = CDO->NetUpdateFrequency;
    const float StoredMin = CDO->MinNetUpdateFrequency;
#endif

    return SaveBlueprintAndReply(Context, Blueprint, FString::Printf(TEXT("Net update frequency %.1f (min: %.1f)"), StoredFrequency, StoredMin), TEXT("Net update frequency configured"));
}
}

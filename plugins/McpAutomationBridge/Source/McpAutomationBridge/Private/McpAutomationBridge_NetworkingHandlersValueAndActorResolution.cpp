#include "McpAutomationBridge_NetworkingHandlersPrivate.h"

namespace McpNetworkingHandlers
{
FString GetStringField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, const FString& Default)
{
    FString Value = Default;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(FieldName, Value);
    }
    return Value;
}

double GetNumberField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, double Default)
{
    double Value = Default;
    if (Payload.IsValid())
    {
        Payload->TryGetNumberField(FieldName, Value);
    }
    return Value;
}

bool GetBoolField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName, bool Default)
{
    bool Value = Default;
    if (Payload.IsValid())
    {
        Payload->TryGetBoolField(FieldName, Value);
    }
    return Value;
}

UBlueprint* LoadBlueprintFromPath(const FString& BlueprintPath)
{
    FString CleanPath = BlueprintPath;
    if (CleanPath.EndsWith(TEXT("_C")))
    {
        return nullptr;
    }

    UBlueprint* Blueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *CleanPath));
    if (Blueprint)
    {
        return Blueprint;
    }

    if (CleanPath.EndsWith(TEXT(".uasset")))
    {
        CleanPath = CleanPath.LeftChop(7);
        Blueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), nullptr, *CleanPath));
    }
    return Blueprint;
}

AActor* FindActorByName(UWorld* World, const FString& ActorName)
{
    if (!World)
    {
        return nullptr;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (Actor && (Actor->GetActorLabel().Equals(ActorName, ESearchCase::IgnoreCase) ||
                      Actor->GetName().Equals(ActorName, ESearchCase::IgnoreCase)))
        {
            return Actor;
        }
    }
    return nullptr;
}

ELifetimeCondition GetReplicationCondition(const FString& ConditionStr)
{
    if (ConditionStr == TEXT("COND_None")) return COND_None;
    if (ConditionStr == TEXT("COND_InitialOnly")) return COND_InitialOnly;
    if (ConditionStr == TEXT("COND_OwnerOnly")) return COND_OwnerOnly;
    if (ConditionStr == TEXT("COND_SkipOwner")) return COND_SkipOwner;
    if (ConditionStr == TEXT("COND_SimulatedOnly")) return COND_SimulatedOnly;
    if (ConditionStr == TEXT("COND_AutonomousOnly")) return COND_AutonomousOnly;
    if (ConditionStr == TEXT("COND_SimulatedOrPhysics")) return COND_SimulatedOrPhysics;
    if (ConditionStr == TEXT("COND_InitialOrOwner")) return COND_InitialOrOwner;
    if (ConditionStr == TEXT("COND_Custom")) return COND_Custom;
    if (ConditionStr == TEXT("COND_ReplayOrOwner")) return COND_ReplayOrOwner;
    if (ConditionStr == TEXT("COND_ReplayOnly")) return COND_ReplayOnly;
    if (ConditionStr == TEXT("COND_SimulatedOnlyNoReplay")) return COND_SimulatedOnlyNoReplay;
    if (ConditionStr == TEXT("COND_SimulatedOrPhysicsNoReplay")) return COND_SimulatedOrPhysicsNoReplay;
    if (ConditionStr == TEXT("COND_SkipReplay")) return COND_SkipReplay;
    if (ConditionStr == TEXT("COND_Never")) return COND_Never;
    return COND_None;
}

ENetDormancy GetNetDormancy(const FString& DormancyStr)
{
    if (DormancyStr == TEXT("DORM_Never")) return DORM_Never;
    if (DormancyStr == TEXT("DORM_Awake")) return DORM_Awake;
    if (DormancyStr == TEXT("DORM_DormantAll")) return DORM_DormantAll;
    if (DormancyStr == TEXT("DORM_DormantPartial")) return DORM_DormantPartial;
    if (DormancyStr == TEXT("DORM_Initial")) return DORM_Initial;
    return DORM_Never;
}

ENetRole GetNetRole(const FString& RoleStr)
{
    if (RoleStr == TEXT("ROLE_None")) return ROLE_None;
    if (RoleStr == TEXT("ROLE_SimulatedProxy")) return ROLE_SimulatedProxy;
    if (RoleStr == TEXT("ROLE_AutonomousProxy")) return ROLE_AutonomousProxy;
    if (RoleStr == TEXT("ROLE_Authority")) return ROLE_Authority;
    return ROLE_None;
}

FString NetRoleToString(ENetRole Role)
{
    switch (Role)
    {
        case ROLE_None: return TEXT("ROLE_None");
        case ROLE_SimulatedProxy: return TEXT("ROLE_SimulatedProxy");
        case ROLE_AutonomousProxy: return TEXT("ROLE_AutonomousProxy");
        case ROLE_Authority: return TEXT("ROLE_Authority");
        default: return TEXT("ROLE_Unknown");
    }
}

FString NetDormancyToString(ENetDormancy Dormancy)
{
    switch (Dormancy)
    {
        case DORM_Never: return TEXT("DORM_Never");
        case DORM_Awake: return TEXT("DORM_Awake");
        case DORM_DormantAll: return TEXT("DORM_DormantAll");
        case DORM_DormantPartial: return TEXT("DORM_DormantPartial");
        case DORM_Initial: return TEXT("DORM_Initial");
        default: return TEXT("DORM_Unknown");
    }
}
}

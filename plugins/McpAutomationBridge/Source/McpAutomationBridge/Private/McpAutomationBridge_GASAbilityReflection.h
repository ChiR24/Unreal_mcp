#pragma once

#include "McpAutomationBridge_GASAvailability.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "UObject/UnrealType.h"

namespace McpGASHandlers
{
static inline FGameplayTag GetOrRequestTag(const FString& TagString)
{
    return FGameplayTag::RequestGameplayTag(FName(*TagString), false);
}

template<typename T>
static bool SetAbilityPropertyValue(UGameplayAbility* Ability, const FName& PropertyName, const T& Value)
{
    if (!Ability)
    {
        return false;
    }

    FProperty* Property = Ability->GetClass()->FindPropertyByName(PropertyName);
    if (!Property)
    {
        return false;
    }

    void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Ability);
    if (!ValuePtr)
    {
        return false;
    }

    *static_cast<T*>(ValuePtr) = Value;
    return true;
}

template<typename T>
static bool GetAbilityPropertyValue(const UGameplayAbility* Ability, const FName& PropertyName, T& OutValue)
{
    if (!Ability)
    {
        return false;
    }

    FProperty* Property = Ability->GetClass()->FindPropertyByName(PropertyName);
    if (!Property)
    {
        return false;
    }

    const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Ability);
    if (!ValuePtr)
    {
        return false;
    }

    OutValue = *static_cast<const T*>(ValuePtr);
    return true;
}

static inline bool AddTagToAbilityContainer(
    UGameplayAbility* Ability,
    const FName& PropertyName,
    const FGameplayTag& Tag)
{
    if (!Ability || !Tag.IsValid())
    {
        return false;
    }

    FProperty* Property = Ability->GetClass()->FindPropertyByName(PropertyName);
    FStructProperty* StructProperty = CastField<FStructProperty>(Property);
    if (!StructProperty || StructProperty->Struct != FGameplayTagContainer::StaticStruct())
    {
        return false;
    }

    void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Ability);
    if (!ValuePtr)
    {
        return false;
    }

    static_cast<FGameplayTagContainer*>(ValuePtr)->AddTag(Tag);
    return true;
}
}
#endif

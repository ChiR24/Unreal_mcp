#pragma once

#include "Domains/GAS/McpAutomationBridge_GASAvailability.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

namespace McpGASHandlers
{
// Where an ability class lands inside an ability-set object: the array
// property plus, for struct-element arrays (FAbilitySet_GameplayAbility and
// friends), the class-typed member written on each new element.
struct FGASAbilityArrayTarget
{
    FArrayProperty* Array = nullptr;
    FProperty* ClassMember = nullptr;

    bool IsValid() const { return Array != nullptr; }

    FString Describe() const
    {
        if (!Array)
        {
            return FString();
        }
        return ClassMember ? Array->GetName() + TEXT("[].") + ClassMember->GetName() : Array->GetName();
    }
};

// True for a class or soft-class reference whose MetaClass accepts AbilityClass.
static inline bool IsAbilityClassProperty(FProperty* Property, UClass* AbilityClass)
{
    if (FClassProperty* ClassProp = CastField<FClassProperty>(Property))
    {
        return !ClassProp->MetaClass || AbilityClass->IsChildOf(ClassProp->MetaClass);
    }
    if (FSoftClassProperty* SoftProp = CastField<FSoftClassProperty>(Property))
    {
        return !SoftProp->MetaClass || AbilityClass->IsChildOf(SoftProp->MetaClass);
    }
    return false;
}

static inline FProperty* FindAbilityClassMember(const UStruct* Type, UClass* AbilityClass)
{
    for (TFieldIterator<FProperty> It(Type); It; ++It)
    {
        if (IsAbilityClassProperty(*It, AbilityClass))
        {
            return *It;
        }
    }
    return nullptr;
}

// Finds the array that should hold granted ability classes: the first array
// whose element (or a member of its struct element) is a class reference
// AbilityClass satisfies, preferring names containing "Abilit". Every array
// property seen is appended to OutArrayNames for the PROPERTY_NOT_FOUND hint.
static inline FGASAbilityArrayTarget FindAbilityArrayTarget(const UStruct* Type, UClass* AbilityClass, TArray<FString>& OutArrayNames)
{
    FGASAbilityArrayTarget Preferred;
    FGASAbilityArrayTarget Fallback;
    for (TFieldIterator<FArrayProperty> It(Type); It; ++It)
    {
        FArrayProperty* ArrayProp = *It;
        OutArrayNames.Add(ArrayProp->GetName());

        FGASAbilityArrayTarget Candidate;
        if (IsAbilityClassProperty(ArrayProp->Inner, AbilityClass))
        {
            Candidate.Array = ArrayProp;
        }
        else if (FStructProperty* StructInner = CastField<FStructProperty>(ArrayProp->Inner))
        {
            if (FProperty* Member = FindAbilityClassMember(StructInner->Struct, AbilityClass))
            {
                Candidate.Array = ArrayProp;
                Candidate.ClassMember = Member;
            }
        }
        if (!Candidate.IsValid())
        {
            continue;
        }

        if (ArrayProp->GetName().Contains(TEXT("Abilit")))
        {
            if (!Preferred.IsValid())
            {
                Preferred = Candidate;
            }
        }
        else if (!Fallback.IsValid())
        {
            Fallback = Candidate;
        }
    }
    return Preferred.IsValid() ? Preferred : Fallback;
}

static inline bool AbilityReferenceEquals(FProperty* Property, const void* ValuePtr, UClass* AbilityClass)
{
    if (FSoftObjectProperty* SoftProp = CastField<FSoftObjectProperty>(Property))
    {
        return SoftProp->GetPropertyValue(ValuePtr).ToSoftObjectPath() == FSoftObjectPath(AbilityClass);
    }
    if (FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property))
    {
        return ObjectProp->GetObjectPropertyValue(ValuePtr) == AbilityClass;
    }
    return false;
}

// Appends AbilityClass to the target array unless an equal reference is
// already present. Returns the element count afterwards.
static inline int32 AppendAbilityClass(void* Container, const FGASAbilityArrayTarget& Target, UClass* AbilityClass, bool& bOutAlreadyPresent)
{
    bOutAlreadyPresent = false;
    FScriptArrayHelper Helper(Target.Array, Target.Array->ContainerPtrToValuePtr<void>(Container));
    FProperty* RefProperty = Target.ClassMember ? Target.ClassMember : Target.Array->Inner;
    auto RefPtr = [&Target](void* Element) -> void*
    {
        return Target.ClassMember ? Target.ClassMember->ContainerPtrToValuePtr<void>(Element) : Element;
    };

    for (int32 Index = 0; Index < Helper.Num(); ++Index)
    {
        if (AbilityReferenceEquals(RefProperty, RefPtr(Helper.GetRawPtr(Index)), AbilityClass))
        {
            bOutAlreadyPresent = true;
            return Helper.Num();
        }
    }

    const int32 NewIndex = Helper.AddValue();
    if (FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(RefProperty))
    {
        ObjectProp->SetObjectPropertyValue(RefPtr(Helper.GetRawPtr(NewIndex)), AbilityClass);
    }
    return Helper.Num();
}
}
#endif

#pragma once

#include "McpAutomationBridge_GASAvailability.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "Engine/Blueprint.h"
#include "GameplayEffect.h"

namespace McpGASHandlers
{
static inline UClass* ResolveGameplayEffectClassFromPath(const FString& EffectPath)
{
    if (EffectPath.IsEmpty())
    {
        return nullptr;
    }

    TArray<FString> ClassPathCandidates;
    ClassPathCandidates.Add(EffectPath);
    if (EffectPath.Contains(TEXT(".")))
    {
        ClassPathCandidates.Add(EffectPath.EndsWith(TEXT("_C")) ? EffectPath : EffectPath + TEXT("_C"));
    }
    else
    {
        int32 LastSlash = INDEX_NONE;
        EffectPath.FindLastChar(TEXT('/'), LastSlash);
        const FString AssetName = LastSlash == INDEX_NONE ? EffectPath : EffectPath.Mid(LastSlash + 1);
        ClassPathCandidates.Add(EffectPath + TEXT(".") + AssetName + TEXT("_C"));
    }

    for (const FString& ClassPath : ClassPathCandidates)
    {
        if (UClass* LoadedClass = LoadClass<UGameplayEffect>(nullptr, *ClassPath))
        {
            if (LoadedClass->IsChildOf(UGameplayEffect::StaticClass()))
            {
                return LoadedClass;
            }
        }
    }

    TArray<FString> ObjectPathCandidates;
    ObjectPathCandidates.Add(EffectPath);
    if (!EffectPath.Contains(TEXT(".")))
    {
        int32 LastSlash = INDEX_NONE;
        EffectPath.FindLastChar(TEXT('/'), LastSlash);
        const FString AssetName = LastSlash == INDEX_NONE ? EffectPath : EffectPath.Mid(LastSlash + 1);
        ObjectPathCandidates.Add(EffectPath + TEXT(".") + AssetName);
    }

    for (const FString& ObjectPath : ObjectPathCandidates)
    {
        if (UBlueprint* EffectBlueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath))
        {
            if (EffectBlueprint->GeneratedClass &&
                EffectBlueprint->GeneratedClass->IsChildOf(UGameplayEffect::StaticClass()))
            {
                return EffectBlueprint->GeneratedClass;
            }
        }
    }

    return nullptr;
}
}
#endif

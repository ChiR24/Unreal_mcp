#pragma once

#include "Domains/GAS/McpAutomationBridge_GASAvailability.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "Engine/Blueprint.h"
#include "Foundation/BridgeHelpers/Reflection/McpAutomationBridgeHelpersClassResolution.h"
#include "GameplayEffect.h"

namespace McpGASHandlers
{
// Resolves a class derived from RequiredBase from any shape callers pass:
// "/Script/Module.Class", a Blueprint package path ("/Game/X/GEC"), an object
// path ("/Game/X/GEC.GEC"), a generated-class path ("/Game/X/GEC.GEC_C") or a
// bare class name. create_execution_calculation hands back the package path,
// which LoadClass alone rejects because the object it names is the UBlueprint.
static inline UClass* ResolveClassFromAssetOrScriptPath(const FString& InPath, UClass* RequiredBase)
{
    FString ClassPath = InPath;
    ClassPath.TrimStartAndEndInline();
    if (ClassPath.IsEmpty() || !RequiredBase)
    {
        return nullptr;
    }

    auto Accept = [RequiredBase](UClass* Candidate) -> UClass*
    {
        return (Candidate && Candidate->IsChildOf(RequiredBase)) ? Candidate : nullptr;
    };

    if (ClassPath.StartsWith(TEXT("/Script/")))
    {
        return Accept(LoadClass<UObject>(nullptr, *ClassPath));
    }

    if (ClassPath.StartsWith(TEXT("/")))
    {
        FString PackagePath = ClassPath;
        FString ObjectName;
        int32 DotIndex = INDEX_NONE;
        if (ClassPath.FindChar(TEXT('.'), DotIndex))
        {
            PackagePath = ClassPath.Left(DotIndex);
            ObjectName = ClassPath.Mid(DotIndex + 1);
            if (ObjectName.EndsWith(TEXT("_C")))
            {
                ObjectName.LeftChopInline(2);
            }
        }
        else
        {
            int32 LastSlash = INDEX_NONE;
            ClassPath.FindLastChar(TEXT('/'), LastSlash);
            ObjectName = ClassPath.Mid(LastSlash + 1);
        }

        if (UClass* Generated = Accept(LoadClass<UObject>(nullptr, *(PackagePath + TEXT(".") + ObjectName + TEXT("_C")))))
        {
            return Generated;
        }
        if (UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *(PackagePath + TEXT(".") + ObjectName)))
        {
            return Accept(Blueprint->GeneratedClass);
        }
        return nullptr;
    }

    return Accept(ResolveClassByName(ClassPath));
}

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

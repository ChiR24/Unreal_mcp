#include "McpAutomationBridge_PCGHandlersPrivate.h"

#if WITH_EDITOR && MCP_HAS_PCG
namespace McpPCGHandlers
{
UClass* ResolvePCGSettingsClass(const FString& RawClassName)
{
    if (RawClassName.IsEmpty())
    {
        return nullptr;
    }

    if (RawClassName.Equals(TEXT("subgraph"), ESearchCase::IgnoreCase) || RawClassName.Equals(TEXT("pcg_subgraph"), ESearchCase::IgnoreCase))
    {
        return UPCGSubgraphSettings::StaticClass();
    }

    TArray<FString> Candidates;
    const FString Trimmed = RawClassName.TrimStartAndEnd();
    Candidates.Add(Trimmed);

    if (const FPCGSettingsAlias* Alias = FindPCGSettingsAlias(Trimmed))
    {
        Candidates.Add(Alias->SettingsClass);
    }

    FString ShortName = Trimmed;
    int32 DotIndex = INDEX_NONE;
    if (ShortName.FindLastChar(TEXT('.'), DotIndex))
    {
        ShortName = ShortName.RightChop(DotIndex + 1);
    }
    if (ShortName.StartsWith(TEXT("U")) && ShortName.StartsWith(TEXT("UPCG")))
    {
        ShortName = ShortName.RightChop(1);
    }

    Candidates.Add(ShortName);
    if (!ShortName.EndsWith(TEXT("Settings")))
    {
        Candidates.Add(ShortName + TEXT("Settings"));
    }
    if (!ShortName.StartsWith(TEXT("PCG")))
    {
        Candidates.Add(TEXT("PCG") + ShortName);
        Candidates.Add(TEXT("PCG") + ShortName + TEXT("Settings"));
    }

    for (const FString& Candidate : Candidates)
    {
        if (UClass* Class = ResolveClassByName(Candidate))
        {
            if (Class->IsChildOf(UPCGSettings::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
            {
                return Class;
            }
        }

        static const TCHAR* ScriptModules[] = {TEXT("PCG"), TEXT("PCGGeometryScriptInterop")};
        for (const TCHAR* ScriptModule : ScriptModules)
        {
            const FString ScriptPath = FString::Printf(TEXT("/Script/%s.%s"), ScriptModule, *Candidate);
            if (UClass* Class = FindObject<UClass>(nullptr, *ScriptPath))
            {
                if (Class->IsChildOf(UPCGSettings::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
                {
                    return Class;
                }
            }
            if (UClass* Class = LoadObject<UClass>(nullptr, *ScriptPath))
            {
                if (Class->IsChildOf(UPCGSettings::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
                {
                    return Class;
                }
            }
        }
    }

    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* Class = *It;
        if (Class && Class->IsChildOf(UPCGSettings::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
        {
            for (const FString& Candidate : Candidates)
            {
                if (Class->GetName().Equals(Candidate, ESearchCase::IgnoreCase))
                {
                    return Class;
                }
            }
        }
    }

    return nullptr;
}

bool ApplySettingsObject(UPCGSettings* Settings, const TSharedPtr<FJsonObject>& SettingsObject, FString& OutError, int32& OutAppliedCount)
{
    OutAppliedCount = 0;
    if (!Settings || !SettingsObject.IsValid())
    {
        return true;
    }

    for (const auto& Pair : SettingsObject->Values)
    {
        const FString FieldName(Pair.Key.Len(), *Pair.Key);
        FProperty* Property = Settings->GetClass()->FindPropertyByName(FName(*FieldName));
        if (!Property)
        {
            OutError = FString::Printf(TEXT("PCG settings property '%s' was not found on '%s'."), *FieldName, *Settings->GetClass()->GetName());
            return false;
        }

        FString ApplyError;
        if (!ApplyJsonValueToProperty(Settings, Property, Pair.Value, ApplyError))
        {
            OutError = FString::Printf(TEXT("Failed to apply PCG settings property '%s': %s"), *FieldName, *ApplyError);
            return false;
        }
        ++OutAppliedCount;
    }

    return true;
}

bool ApplyStringSetting(UPCGSettings* Settings, const TCHAR* PropertyName, const FString& Value, FString& OutError)
{
    if (!Settings || Value.IsEmpty())
    {
        return true;
    }

    FProperty* Property = Settings->GetClass()->FindPropertyByName(FName(PropertyName));
    if (!Property)
    {
        OutError = FString::Printf(TEXT("PCG settings property '%s' was not found on '%s'."), PropertyName, *Settings->GetClass()->GetName());
        return false;
    }

    return ApplyJsonValueToProperty(Settings, Property, MakeShared<FJsonValueString>(Value), OutError);
}

bool ResolveClassForProperty(UObject* Target, const TCHAR* PropertyName, const FString& ClassName, UClass*& OutClass, FString& OutError)
{
    OutClass = nullptr;
    if (!Target || ClassName.IsEmpty())
    {
        return true;
    }

    FProperty* Property = Target->GetClass()->FindPropertyByName(FName(PropertyName));
    FClassProperty* ClassProperty = CastField<FClassProperty>(Property);
    if (!ClassProperty)
    {
        OutError = FString::Printf(TEXT("Class property '%s' was not found on '%s'."), PropertyName, *Target->GetClass()->GetName());
        return false;
    }

    UClass* Class = ResolveClassByName(ClassName);
    if (!Class)
    {
        Class = LoadObject<UClass>(nullptr, *ClassName);
    }
    if (!Class && ClassName.StartsWith(TEXT("/Script/")))
    {
        Class = FindObject<UClass>(nullptr, *ClassName);
    }
    if (!Class)
    {
        OutError = FString::Printf(TEXT("Could not resolve class '%s'."), *ClassName);
        return false;
    }
    if (ClassProperty->MetaClass && !Class->IsChildOf(ClassProperty->MetaClass))
    {
        OutError = FString::Printf(TEXT("Class '%s' is not assignable to '%s'."), *Class->GetName(), *ClassProperty->MetaClass->GetName());
        return false;
    }

    OutClass = Class;
    return true;
}
}
#endif

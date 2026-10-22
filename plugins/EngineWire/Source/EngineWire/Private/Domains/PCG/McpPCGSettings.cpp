#include "Domains/PCG/McpPCGPrivate.h"
#include "Foundation/BridgeHelpers/Properties/McpNestedPropPath.h"
#include "Foundation/Reflection/McpPropertyReflection.h"
#include "Domains/PCG/Spawning/McpPCGSpawnerTypes.h"

#if MCP_HAS_PCG
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
#include "Elements/ControlFlow/PCGSwitch.h"
#endif

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

    if (const TCHAR* SettingsClass = FindPCGSettingsAlias(Trimmed))
    {
        Candidates.Add(SettingsClass);
    }

    FString ShortName = Trimmed;
    int32 DotIndex = INDEX_NONE;
    if (ShortName.FindLastChar(TEXT('.'), DotIndex))
    {
        ShortName = ShortName.RightChop(DotIndex + 1);
    }
    if (ShortName.StartsWith(TEXT("UPCG")))
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

    // ResolveClassByName already finds a loaded class by short or full name (native PCG modules
    // register their classes on load), so one pass per spelling is enough.
    for (const FString& Candidate : Candidates)
    {
        UClass* Class = ResolveClassByName(Candidate);
        if (Class && Class->IsChildOf(UPCGSettings::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
        {
            return Class;
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

    TSet<FString> HandledFields;
    if (!ApplyPCGSpawnerTypes(Settings, SettingsObject, HandledFields, OutError)) return false;
    OutAppliedCount = HandledFields.Num();
    for (const auto& Pair : SettingsObject->Values)
    {
        const FString FieldName(Pair.Key.Len(), *Pair.Key);
        if (HandledFields.Contains(FieldName)) continue;
        void* Container = nullptr;
        FString ResolvedPath;
        FProperty* Property = McpResolvePropertyPath(Settings, FieldName, Container, ResolvedPath, OutError);
        if (!Property) return false;
        TArray<FString> Segments;
        ResolvedPath.ParseIntoArray(Segments, TEXT("."), true);
        FString Prefix;
        for (int32 Index = 0; Index + 1 < Segments.Num(); ++Index)
        {
            Prefix += (Prefix.IsEmpty() ? TEXT("") : TEXT(".")) + Segments[Index];
            void* PrefixContainer = nullptr;
            FString Ignored;
            FProperty* PrefixProperty = McpResolvePropertyPath(Settings, Prefix, PrefixContainer, Ignored, OutError);
            if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(PrefixProperty))
            {
                UObject* Object = ObjectProperty->GetObjectPropertyValue_InContainer(PrefixContainer);
                if (Object && Object != Settings && !Object->IsIn(Settings))
                {
                    OutError = TEXT("Nested PCG settings writes may only traverse objects owned by these settings, not shared assets or CDOs.");
                    return false;
                }
            }
        }
        FString ApplyError;
        if (!::ApplyJsonValueToProperty(Container, Property, Pair.Value, ApplyError))
        {
            OutError = FString::Printf(TEXT("Failed to apply PCG settings property '%s': %s"), *FieldName, *ApplyError);
            return false;
        }
        ++OutAppliedCount;
    }

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
    // Native direct edits must refresh the selection cache as well as the visible pins.
    if (OutAppliedCount > 0 && Settings->GetClass() == UPCGSwitchSettings::StaticClass())
        static_cast<UPCGSwitchSettings*>(Settings)->UpdateSettings();
#endif
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

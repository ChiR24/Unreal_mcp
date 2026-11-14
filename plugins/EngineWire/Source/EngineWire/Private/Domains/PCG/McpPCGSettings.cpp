#include "Domains/PCG/McpPCGPrivate.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"

#if MCP_HAS_PCG
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

    // Every property is found before any is written: a key the class lacks left the ones before it changed in memory,
    // with no PostEditChange, notification or save.
    TArray<TPair<FProperty*, TSharedPtr<FJsonValue>>> Writes;
    for (const auto& Pair : SettingsObject->Values)
    {
        const FString FieldName = McpJsonKey(Pair.Key);
        FProperty* Property = Settings->GetClass()->FindPropertyByName(FName(*FieldName));
        if (!Property)
        {
            OutError = FString::Printf(TEXT("PCG settings property '%s' was not found on '%s'. Nothing was changed."), *FieldName, *Settings->GetClass()->GetName());
            return false;
        }
        Writes.Emplace(Property, Pair.Value);
    }
    for (const TPair<FProperty*, TSharedPtr<FJsonValue>>& Write : Writes)
    {
        FString ApplyError;
        if (!ApplyJsonValueToProperty(Settings, Write.Key, Write.Value, ApplyError))
        {
            OutError = FString::Printf(TEXT("Failed to apply PCG settings property '%s': %s (%d written before it)"),
                *Write.Key->GetName(), *ApplyError, OutAppliedCount);
            return false;
        }
        ++OutAppliedCount;
    }

    return true;
}

bool ApplyStringSetting(UPCGSettings* Settings, const TCHAR* PropertyName, const FString& Value, FString& OutError)
{
    if (Value.IsEmpty())
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
}
#endif

#pragma once

#include "CoreMinimal.h"

namespace McpRenderHandlers
{
#if WITH_EDITOR
inline const TMap<FString, FString>& DepthOfFieldMethodMap()
{
    static const TMap<FString, FString> Map = {
        { TEXT("CircleDOF"),       TEXT("DOFM_CircleDOF") },
        { TEXT("GaussianDOF"),     TEXT("DOFM_Gaussian") },
        { TEXT("BokehDOF"),        TEXT("DOFM_BokehDOF") },
        { TEXT("CinematicDOF"),    TEXT("DOFM_CircleDOF") },
        { TEXT("USGDOF"),          TEXT("DOFM_USG") }
    };
    return Map;
}

inline const TMap<FString, FString>& AutoExposureMethodMap()
{
    static const TMap<FString, FString> Map = {
        { TEXT("Manual"),          TEXT("AEM_Manual") },
        { TEXT("Histogram"),       TEXT("AEM_Histogram") },
        { TEXT("Basic"),           TEXT("AEM_Basic") },
        { TEXT("Recovery"),        TEXT("AEM_Recovery") }
    };
    return Map;
}

inline bool ResolveEnumAlias(
    const TMap<FString, FString>& Map,
    const FString& Input,
    FString& OutValue,
    FString& OutError)
{
    for (const TPair<FString, FString>& Pair : Map)
    {
        if (Pair.Key.Equals(Input, ESearchCase::IgnoreCase))
        {
            OutValue = Pair.Value;
            return true;
        }
    }
    TArray<FString> Allowed;
    Map.GetKeys(Allowed);
    FString AllowedList;
    for (int32 i = 0; i < Allowed.Num(); ++i)
    {
        AllowedList += (i == 0 ? FString() : FString(TEXT(", "))) + Allowed[i];
    }
    OutError = FString::Printf(
        TEXT("Unknown method '%s'. Allowed values: %s"),
        *Input, *AllowedList);
    return false;
}
#endif
}

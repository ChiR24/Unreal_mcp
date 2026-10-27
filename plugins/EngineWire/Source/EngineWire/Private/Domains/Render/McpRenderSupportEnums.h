#pragma once

#include "CoreMinimal.h"

namespace McpRenderHandlers
{
// The one depth of field choice a volume holds since UE 5: desktop and console always render the cinematic
// (diaphragm) depth of field, and mobile renders either its own or the high quality Gaussian one, which is
// bMobileHQGaussian. The method used to be written to DepthOfFieldMethod, a field UE 5 no longer has, so every call
// was refused. The UE 4 names are taken as before: CircleDOF and BokehDOF are the cinematic one, GaussianDOF Gaussian.
inline bool ResolveDepthOfFieldMethod(const FString& Input, bool& bOutMobileHQGaussian, FString& OutError)
{
    for (const TCHAR* Name : {TEXT("Cinematic"), TEXT("CinematicDOF"), TEXT("CircleDOF"), TEXT("BokehDOF"), TEXT("Gaussian"), TEXT("GaussianDOF")})
    {
        if (Input.Equals(Name, ESearchCase::IgnoreCase))
        {
            bOutMobileHQGaussian = Input.StartsWith(TEXT("Gaussian"), ESearchCase::IgnoreCase);
            return true;
        }
    }
    OutError = FString::Printf(TEXT("Unknown method '%s'. Allowed values: Cinematic (also CircleDOF, BokehDOF), Gaussian"), *Input);
    return false;
}

// EAutoExposureMethod has no Recovery value in any UE 5, so that name wrote a value the renderer does not know.
inline const TMap<FString, FString>& AutoExposureMethodMap()
{
    static const TMap<FString, FString> Map = {
        { TEXT("Manual"),          TEXT("AEM_Manual") },
        { TEXT("Histogram"),       TEXT("AEM_Histogram") },
        { TEXT("Basic"),           TEXT("AEM_Basic") }
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
}

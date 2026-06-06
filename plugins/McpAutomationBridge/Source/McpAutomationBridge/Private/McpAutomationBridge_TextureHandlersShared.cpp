#include "McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
namespace
{
constexpr int32 MaxGeneratedTextureDimension = 4096;
constexpr int64 MaxGeneratedTexturePixels = 2048LL * 2048LL;

float Noise2D(float X, float Y, int32 Seed)
{
    const int32 IntX = FMath::FloorToInt(X);
    const int32 IntY = FMath::FloorToInt(Y);
    const float FracX = X - IntX;
    const float FracY = Y - IntY;
    auto Hash = [Seed](int32 HashX, int32 HashY) -> float
    {
        int32 N = HashX + HashY * 57 + Seed * 131;
        N = (N << 13) ^ N;
        return 1.0f - ((N * (N * N * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
    };
    const float SmoothX = FracX * FracX * (3.0f - 2.0f * FracX);
    const float SmoothY = FracY * FracY * (3.0f - 2.0f * FracY);
    const float I0 = FMath::Lerp(Hash(IntX, IntY), Hash(IntX + 1, IntY), SmoothX);
    const float I1 = FMath::Lerp(Hash(IntX, IntY + 1), Hash(IntX + 1, IntY + 1), SmoothX);
    return FMath::Lerp(I0, I1, SmoothY);
}
}

FString GetStringField(const TSharedPtr<FJsonObject>& Obj, const FString& FieldName, const FString& Default)
{
    return McpHandlerUtils::GetOptionalString(Obj, FieldName, Default);
}

double GetNumberField(const TSharedPtr<FJsonObject>& Obj, const FString& FieldName, double Default)
{
    double Value = Default;
    if (Obj.IsValid())
    {
        Obj->TryGetNumberField(FieldName, Value);
    }
    return Value;
}

bool GetBoolField(const TSharedPtr<FJsonObject>& Obj, const FString& FieldName, bool Default)
{
    return McpHandlerUtils::GetOptionalBool(Obj, FieldName, Default);
}

bool ValidateGeneratedTextureDimensions(double WidthValue, double HeightValue,
                                        const TCHAR* WidthName, const TCHAR* HeightName,
                                        int32& OutWidth, int32& OutHeight,
                                        FString& OutError)
{
    if (!FMath::IsFinite(WidthValue) || !FMath::IsFinite(HeightValue) ||
        WidthValue != FMath::FloorToDouble(WidthValue) || HeightValue != FMath::FloorToDouble(HeightValue))
    {
        OutError = FString::Printf(TEXT("%s and %s must be finite whole numbers"), WidthName, HeightName);
        return false;
    }
    if (WidthValue < 1.0 || HeightValue < 1.0 ||
        WidthValue > static_cast<double>(MaxGeneratedTextureDimension) ||
        HeightValue > static_cast<double>(MaxGeneratedTextureDimension))
    {
        OutError = FString::Printf(TEXT("%s and %s must be between 1 and %d"),
                                   WidthName, HeightName, MaxGeneratedTextureDimension);
        return false;
    }
    const int32 Width = static_cast<int32>(WidthValue);
    const int32 Height = static_cast<int32>(HeightValue);
    if (static_cast<int64>(Width) * static_cast<int64>(Height) > MaxGeneratedTexturePixels)
    {
        OutError = FString::Printf(TEXT("%s x %s exceeds the generated texture pixel limit of %lld"),
                                   WidthName, HeightName, MaxGeneratedTexturePixels);
        return false;
    }
    OutWidth = Width;
    OutHeight = Height;
    return true;
}

bool ValidateTextureIterationCount(double Value, const TCHAR* Name,
                                   int32 MinValue, int32 MaxValue,
                                   int32& OutValue, FString& OutError)
{
    if (!FMath::IsFinite(Value) || Value != FMath::FloorToDouble(Value))
    {
        OutError = FString::Printf(TEXT("%s must be a finite whole number"), Name);
        return false;
    }
    if (Value < static_cast<double>(MinValue) || Value > static_cast<double>(MaxValue))
    {
        OutError = FString::Printf(TEXT("%s must be between %d and %d"), Name, MinValue, MaxValue);
        return false;
    }
    OutValue = static_cast<int32>(Value);
    return true;
}

FString NormalizeTexturePath(const FString& Path)
{
    FString Normalized = Path;
    Normalized.ReplaceInline(TEXT("/Content"), TEXT("/Game"));
    Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
    while (Normalized.EndsWith(TEXT("/")))
    {
        Normalized.LeftChopInline(1);
    }
    return Normalized;
}

FAssetData GetTextureAssetDataByObjectPath(const FString& ObjectPath)
{
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    return AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(ObjectPath));
#else
    return AssetRegistry.GetAssetByObjectPath(FName(*ObjectPath));
#endif
}

float FBMNoise(float X, float Y, int32 Octaves, float Persistence, float Lacunarity, int32 Seed)
{
    float Total = 0.0f;
    float Amplitude = 1.0f;
    float Frequency = 1.0f;
    float MaxValue = 0.0f;
    for (int32 Index = 0; Index < Octaves; ++Index)
    {
        Total += Noise2D(X * Frequency, Y * Frequency, Seed + Index) * Amplitude;
        MaxValue += Amplitude;
        Amplitude *= Persistence;
        Frequency *= Lacunarity;
    }
    return Total / MaxValue;
}
}

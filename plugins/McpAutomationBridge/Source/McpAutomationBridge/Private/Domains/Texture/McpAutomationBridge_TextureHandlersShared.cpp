#include "Domains/Texture/McpAutomationBridge_TextureHandlersShared.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersAssetPathCanonical.h"

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

// Empty means REFUSED, not "unchanged": the shared canonicalizer rejects a
// value whose resolved target it cannot vouch for. Replaying its steps here in
// a different order (alias map before separator normalization) is what let
// "\Content\..." mean one thing to the pre-queue gate and another here.
FString NormalizeTexturePath(const FString& Path)
{
    return McpCanonicalizeContentPath(Path, /*bAssumeGameRoot=*/true);
}

UTexture2D* LoadSourceTexture(const FString& RawPath, const TCHAR* Field, FString& OutPath, FString& OutError)
{
    OutPath = NormalizeTexturePath(RawPath);
    UTexture2D* Texture = OutPath.IsEmpty() ? nullptr : LoadObject<UTexture2D>(nullptr, *OutPath);
    if (!Texture)
    {
        OutError = RawPath.IsEmpty() ? FString::Printf(TEXT("%s is required"), Field)
                                     : FString::Printf(TEXT("%s: no texture at '%s'"), Field, *RawPath);
        return nullptr;
    }
    if (Texture->Source.GetFormat() != TSF_BGRA8)
    {
        OutError = FString::Printf(TEXT("%s '%s' has no 8-bit BGRA source data"), Field, *OutPath);
        return nullptr;
    }
    return Texture;
}

bool ResolveOutputTarget(const TSharedPtr<FJsonObject>& Params, const FString& DefaultPath, const FString& DefaultName,
                         FString& OutPath, FString& OutName, FString& OutError)
{
    const FString OutputPath = GetJsonStringField(Params, TEXT("outputPath"));
    const FString Path = OutputPath.IsEmpty() ? GetJsonStringField(Params, TEXT("path")) : FPaths::GetPath(OutputPath);
    const FString Name = OutputPath.IsEmpty() ? GetJsonStringField(Params, TEXT("name")) : FPaths::GetBaseFilename(OutputPath);
    OutPath = NormalizeTexturePath(Path.IsEmpty() ? DefaultPath : Path);
    OutName = SanitizeAssetName(Name.IsEmpty() ? DefaultName : Name);
    if (OutPath.IsEmpty() || OutName.IsEmpty())
    {
        OutError = OutPath.IsEmpty() ? TEXT("Invalid output path") : TEXT("name (or outputPath) is required and must be a valid asset name");
        return false;
    }
    return true;
}

TArray<int32> ChannelOffsets(const FString& Channel)
{
    auto Is = [&Channel](const TCHAR* Letter, const TCHAR* Word)
    {
        return Channel.Equals(Letter, ESearchCase::IgnoreCase) || Channel.Equals(Word, ESearchCase::IgnoreCase);
    };
    if (Is(TEXT("R"), TEXT("Red"))) return {2};
    if (Is(TEXT("G"), TEXT("Green"))) return {1};
    if (Is(TEXT("B"), TEXT("Blue"))) return {0};
    if (Is(TEXT("A"), TEXT("Alpha"))) return {3};
    return {0, 1, 2};
}

FAssetData GetTextureAssetDataByObjectPath(const FString& ObjectPath)
{
    return FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(MCP_ASSET_REGISTRY_OBJECT_PATH(ObjectPath));
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

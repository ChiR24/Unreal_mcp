#include "Domains/Texture/McpTextureShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleCreateNoiseTexture(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString Path;
    FString Name;
    FString Error;
    int32 Width = 0;
    int32 Height = 0;
    if (!ReadGeneratedTextureTarget(Params, Path, Name, Width, Height, Error))
    {
        TEXTURE_ERROR_RESPONSE(Error);
    }

    const float Scale = static_cast<float>(GetJsonNumberField(Params, TEXT("scale"), 1.0));
    int32 Octaves = 0;
    if (!ValidateTextureIterationCount(GetJsonNumberField(Params, TEXT("octaves"), 4),
                                       TEXT("octaves"), 1, 16,
                                       Octaves, Error))
    {
        TEXTURE_ERROR_RESPONSE(Error);
    }
    const float Persistence = static_cast<float>(GetJsonNumberField(Params, TEXT("persistence"), 0.5));
    const float Lacunarity = static_cast<float>(GetJsonNumberField(Params, TEXT("lacunarity"), 2.0));
    const int32 Seed = static_cast<int32>(GetJsonNumberField(Params, TEXT("seed"), 0));
    const bool bSeamless = GetJsonBoolField(Params, TEXT("seamless"), false);
    // noiseType was declared and never read (one algorithm always ran). Perlin
    // (fractal Brownian motion) stays the default; ridged and billow fold it.
    const FString NoiseType = GetJsonStringField(Params, TEXT("noiseType"), TEXT("Perlin"));
    const bool bRidged = NoiseType.Equals(TEXT("Ridged"), ESearchCase::IgnoreCase);
    const bool bBillow = NoiseType.Equals(TEXT("Billow"), ESearchCase::IgnoreCase);
    if (!bRidged && !bBillow && !NoiseType.Equals(TEXT("Perlin"), ESearchCase::IgnoreCase) && !NoiseType.Equals(TEXT("FBM"), ESearchCase::IgnoreCase))
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Unknown noiseType '%s'. Valid values: Perlin, FBM, Ridged, Billow."), *NoiseType));
    }

    TArray<uint8> PixelData;
    PixelData.SetNumZeroed(Width * Height * 4);
    for (int32 Y = 0; Y < Height; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            const float NX = static_cast<float>(X) / static_cast<float>(Width) * Scale;
            const float NY = static_cast<float>(Y) / static_cast<float>(Height) * Scale;
            float NoiseValue = 0.0f;
            if (bSeamless)
            {
                const float Angle1 = NX * PI * 2.0f;
                const float Angle2 = NY * PI * 2.0f;
                NoiseValue = FBMNoise(FMath::Cos(Angle1) + FMath::Cos(Angle2),
                                      FMath::Sin(Angle1) + FMath::Sin(Angle2),
                                      Octaves, Persistence, Lacunarity, Seed);
            }
            else
            {
                NoiseValue = FBMNoise(NX, NY, Octaves, Persistence, Lacunarity, Seed);
            }

            if (bRidged) { NoiseValue = 1.0f - 2.0f * FMath::Abs(NoiseValue); }
            else if (bBillow) { NoiseValue = 2.0f * FMath::Abs(NoiseValue) - 1.0f; }
            NoiseValue = FMath::Clamp((NoiseValue + 1.0f) * 0.5f, 0.0f, 1.0f);
            const int32 PixelIndex = (Y * Width + X) * 4;
            const uint8 ByteValue = static_cast<uint8>(NoiseValue * 255.0f);
            PixelData[PixelIndex + 0] = ByteValue;
            PixelData[PixelIndex + 1] = ByteValue;
            PixelData[PixelIndex + 2] = ByteValue;
            PixelData[PixelIndex + 3] = 255;
        }
    }

    return FinishGeneratedTexture(Response, Path, Name, Width, Height, PixelData, TEXT("Noise"));
}
}

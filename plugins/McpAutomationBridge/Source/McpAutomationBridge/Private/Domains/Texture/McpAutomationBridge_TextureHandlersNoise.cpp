#include "Domains/Texture/McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleCreateNoiseTexture(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString Path;
    FString Name;
    FString Error;
    if (!ResolveOutputTarget(Params, TEXT("/Game/Textures"), FString(), Path, Name, Error))
    {
        TEXTURE_ERROR_RESPONSE(Error);
    }

    int32 Width = 0;
    int32 Height = 0;
    if (!ValidateGeneratedTextureDimensions(GetJsonNumberField(Params, TEXT("width"), 1024),
                                            GetJsonNumberField(Params, TEXT("height"), 1024),
                                            TEXT("width"), TEXT("height"),
                                            Width, Height, Error))
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
    const bool bHDR = GetJsonBoolField(Params, TEXT("hdr"), false);


    UTexture2D* NewTexture = CreateEmptyTexture(Path, Name, Width, Height, bHDR);
    if (!NewTexture)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to create texture"));
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

            NoiseValue = FMath::Clamp((NoiseValue + 1.0f) * 0.5f, 0.0f, 1.0f);
            const int32 PixelIndex = (Y * Width + X) * 4;
            const uint8 ByteValue = static_cast<uint8>(NoiseValue * 255.0f);
            PixelData[PixelIndex + 0] = ByteValue;
            PixelData[PixelIndex + 1] = ByteValue;
            PixelData[PixelIndex + 2] = ByteValue;
            PixelData[PixelIndex + 3] = 255;
        }
    }

    if (!UpdateTextureBGRA8(NewTexture, Width, Height, PixelData))
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to update texture pixel data"));
    }
    if (!SaveTextureAsset(NewTexture))
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to save noise texture"));
    }

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Noise texture '%s' created"), *Name));
    McpHandlerUtils::AddVerification(Response, NewTexture);
    return Response;
}
}

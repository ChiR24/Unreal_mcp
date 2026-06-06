#include "McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleCreateNoiseTexture(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    TSet<FString> ValidParams = {
        TEXT("subAction"), TEXT("name"), TEXT("path"), TEXT("noiseType"),
        TEXT("width"), TEXT("height"), TEXT("scale"), TEXT("octaves"),
        TEXT("persistence"), TEXT("lacunarity"), TEXT("seed"),
        TEXT("seamless"), TEXT("hdr"), TEXT("save")
    };
    for (const auto& Field : Params->Values)
    {
        if (!ValidParams.Contains(FString(*Field.Key)))
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Invalid parameter: %s"), *Field.Key));
        }
    }

    FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
    FString Path = GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures"));
    FString SanitizedPath = SanitizeProjectRelativePath(Path);
    if (SanitizedPath.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid path: contains traversal or invalid characters"));
    }
    Path = SanitizedPath;

    FString SanitizedName = SanitizeAssetName(Name);
    if (SanitizedName.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid name: contains invalid characters"));
    }
    Name = SanitizedName;

    int32 Width = 0;
    int32 Height = 0;
    FString ValidationError;
    if (!ValidateGeneratedTextureDimensions(GetNumberFieldTextAuth(Params, TEXT("width"), 1024),
                                            GetNumberFieldTextAuth(Params, TEXT("height"), 1024),
                                            TEXT("width"), TEXT("height"),
                                            Width, Height, ValidationError))
    {
        TEXTURE_ERROR_RESPONSE(ValidationError);
    }

    const float Scale = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("scale"), 1.0));
    int32 Octaves = 0;
    if (!ValidateTextureIterationCount(GetNumberFieldTextAuth(Params, TEXT("octaves"), 4),
                                       TEXT("octaves"), 1, 16,
                                       Octaves, ValidationError))
    {
        TEXTURE_ERROR_RESPONSE(ValidationError);
    }
    const float Persistence = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("persistence"), 0.5));
    const float Lacunarity = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("lacunarity"), 2.0));
    const int32 Seed = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("seed"), 0));
    const bool bSeamless = GetBoolFieldTextAuth(Params, TEXT("seamless"), false);
    const bool bHDR = GetBoolFieldTextAuth(Params, TEXT("hdr"), false);
    const bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);

    if (Name.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Name is required"));
    }

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
    if (bSave && !SaveTextureAsset(NewTexture))
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to save noise texture"));
    }

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Noise texture '%s' created"), *Name));
    McpHandlerUtils::AddVerification(Response, NewTexture);
    return Response;
}
}

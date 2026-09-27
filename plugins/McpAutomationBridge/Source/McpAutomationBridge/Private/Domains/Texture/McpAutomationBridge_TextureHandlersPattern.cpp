#include "Domains/Texture/McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleCreatePatternTexture(const TSharedPtr<FJsonObject>& Params)
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
    int32 TilesX = 0;
    int32 TilesY = 0;
    if (!ValidateGeneratedTextureDimensions(GetJsonNumberField(Params, TEXT("width"), 1024),
                                            GetJsonNumberField(Params, TEXT("height"), 1024),
                                            TEXT("width"), TEXT("height"),
                                            Width, Height, Error) ||
        !ValidateTextureIterationCount(GetJsonNumberField(Params, TEXT("tilesX"), 8),
                                       TEXT("tilesX"), 1, 1024, TilesX, Error) ||
        !ValidateTextureIterationCount(GetJsonNumberField(Params, TEXT("tilesY"), 8),
                                       TEXT("tilesY"), 1, 1024, TilesY, Error))
    {
        TEXTURE_ERROR_RESPONSE(Error);
    }

    const FString PatternType = GetJsonStringField(Params, TEXT("patternType"), TEXT("Checker"));
    const float LineWidth = static_cast<float>(GetJsonNumberField(Params, TEXT("lineWidth"), 0.02));
    const float BrickRatio = static_cast<float>(GetJsonNumberField(Params, TEXT("brickRatio"), 2.0));
    const float Offset = static_cast<float>(GetJsonNumberField(Params, TEXT("offset"), 0.5));
    const FLinearColor PrimaryColor = ExtractLinearColorField(Params, TEXT("primaryColor"), FLinearColor(1, 1, 1, 1));
    const FLinearColor SecondaryColor = ExtractLinearColorField(Params, TEXT("secondaryColor"), FLinearColor(0, 0, 0, 1));


    UTexture2D* NewTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
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
            const float NX = static_cast<float>(X) / static_cast<float>(Width);
            const float NY = static_cast<float>(Y) / static_cast<float>(Height);
            bool bUsePrimary = true;
            if (PatternType == TEXT("Checker"))
            {
                bUsePrimary = ((static_cast<int32>(NX * TilesX) + static_cast<int32>(NY * TilesY)) % 2) == 0;
            }
            else if (PatternType == TEXT("Grid"))
            {
                const float LocalX = FMath::Fmod(NX, 1.0f / TilesX) * TilesX;
                const float LocalY = FMath::Fmod(NY, 1.0f / TilesY) * TilesY;
                bUsePrimary = LocalX > LineWidth && LocalX < 1.0f - LineWidth &&
                              LocalY > LineWidth && LocalY < 1.0f - LineWidth;
            }
            else if (PatternType == TEXT("Brick"))
            {
                const int32 Row = static_cast<int32>(NY * TilesY);
                const float AdjustedX = FMath::Fmod(NX + ((Row % 2 == 1) ? Offset / TilesX : 0.0f), 1.0f);
                const float LocalX = FMath::Fmod(AdjustedX, BrickRatio / TilesX) / (BrickRatio / TilesX);
                const float LocalY = FMath::Fmod(NY, 1.0f / TilesY) * TilesY;
                bUsePrimary = LocalX > LineWidth && LocalX < 1.0f - LineWidth &&
                              LocalY > LineWidth && LocalY < 1.0f - LineWidth;
            }
            else if (PatternType == TEXT("Stripes"))
            {
                bUsePrimary = (static_cast<int32>(NX * TilesX) % 2) == 0;
            }
            else if (PatternType == TEXT("Dots"))
            {
                const float CenterLocalX = FMath::Fmod(NX, 1.0f / TilesX) * TilesX - 0.5f;
                const float CenterLocalY = FMath::Fmod(NY, 1.0f / TilesY) * TilesY - 0.5f;
                bUsePrimary = FMath::Sqrt(CenterLocalX * CenterLocalX + CenterLocalY * CenterLocalY) < 0.3f;
            }

            const FLinearColor Color = bUsePrimary ? PrimaryColor : SecondaryColor;
            const int32 PixelIndex = (Y * Width + X) * 4;
            PixelData[PixelIndex + 0] = static_cast<uint8>(Color.B * 255.0f);
            PixelData[PixelIndex + 1] = static_cast<uint8>(Color.G * 255.0f);
            PixelData[PixelIndex + 2] = static_cast<uint8>(Color.R * 255.0f);
            PixelData[PixelIndex + 3] = static_cast<uint8>(Color.A * 255.0f);
        }
    }

    if (!UpdateTextureBGRA8(NewTexture, Width, Height, PixelData))
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to update texture pixel data"));
    }
    if (!SaveTextureAsset(NewTexture))
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to save pattern texture"));
    }

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Pattern texture '%s' created"), *Name));
    McpHandlerUtils::AddVerification(Response, NewTexture);
    return Response;
}
}

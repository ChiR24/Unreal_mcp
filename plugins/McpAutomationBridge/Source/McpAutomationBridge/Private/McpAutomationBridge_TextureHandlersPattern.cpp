#include "McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
namespace
{
FLinearColor ReadPatternColor(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, const FLinearColor& Default)
{
    FLinearColor Color = Default;
    const TSharedPtr<FJsonObject>* ColorObject = nullptr;
    if (Params->TryGetObjectField(FieldName, ColorObject))
    {
        Color.R = static_cast<float>(GetNumberFieldTextAuth(*ColorObject, TEXT("r"), Color.R));
        Color.G = static_cast<float>(GetNumberFieldTextAuth(*ColorObject, TEXT("g"), Color.G));
        Color.B = static_cast<float>(GetNumberFieldTextAuth(*ColorObject, TEXT("b"), Color.B));
        Color.A = static_cast<float>(GetNumberFieldTextAuth(*ColorObject, TEXT("a"), Color.A));
    }
    return Color;
}
}

TSharedPtr<FJsonObject> HandleCreatePatternTexture(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    TSet<FString> ValidParams = {
        TEXT("subAction"), TEXT("name"), TEXT("path"), TEXT("patternType"),
        TEXT("width"), TEXT("height"), TEXT("tilesX"), TEXT("tilesY"),
        TEXT("lineWidth"), TEXT("brickRatio"), TEXT("offset"), TEXT("save"),
        TEXT("primaryColor"), TEXT("secondaryColor")
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
    int32 TilesX = 0;
    int32 TilesY = 0;
    FString ValidationError;
    if (!ValidateGeneratedTextureDimensions(GetNumberFieldTextAuth(Params, TEXT("width"), 1024),
                                            GetNumberFieldTextAuth(Params, TEXT("height"), 1024),
                                            TEXT("width"), TEXT("height"),
                                            Width, Height, ValidationError) ||
        !ValidateTextureIterationCount(GetNumberFieldTextAuth(Params, TEXT("tilesX"), 8),
                                       TEXT("tilesX"), 1, 1024, TilesX, ValidationError) ||
        !ValidateTextureIterationCount(GetNumberFieldTextAuth(Params, TEXT("tilesY"), 8),
                                       TEXT("tilesY"), 1, 1024, TilesY, ValidationError))
    {
        TEXTURE_ERROR_RESPONSE(ValidationError);
    }

    const FString PatternType = GetStringFieldTextAuth(Params, TEXT("patternType"), TEXT("Checker"));
    const float LineWidth = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("lineWidth"), 0.02));
    const float BrickRatio = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("brickRatio"), 2.0));
    const float Offset = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("offset"), 0.5));
    const bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
    const FLinearColor PrimaryColor = ReadPatternColor(Params, TEXT("primaryColor"), FLinearColor(1, 1, 1, 1));
    const FLinearColor SecondaryColor = ReadPatternColor(Params, TEXT("secondaryColor"), FLinearColor(0, 0, 0, 1));

    if (Name.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Name is required"));
    }

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
    if (bSave && !SaveTextureAsset(NewTexture))
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to save pattern texture"));
    }

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Pattern texture '%s' created"), *Name));
    McpHandlerUtils::AddVerification(Response, NewTexture);
    return Response;
}
}

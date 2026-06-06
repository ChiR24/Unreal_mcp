#include "McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
namespace
{
FLinearColor ReadColor(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, const FLinearColor& Default)
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

TSharedPtr<FJsonObject> HandleCreateGradientTexture(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    TSet<FString> ValidParams = {
        TEXT("subAction"), TEXT("name"), TEXT("path"), TEXT("gradientType"),
        TEXT("width"), TEXT("height"), TEXT("angle"), TEXT("centerX"),
        TEXT("centerY"), TEXT("radius"), TEXT("hdr"), TEXT("save"),
        TEXT("startColor"), TEXT("endColor")
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

    const FString GradientType = GetStringFieldTextAuth(Params, TEXT("gradientType"), TEXT("Linear"));
    const float Angle = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("angle"), 0.0));
    const float CenterX = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("centerX"), 0.5));
    const float CenterY = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("centerY"), 0.5));
    const float Radius = static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("radius"), 0.5));
    if (!FMath::IsFinite(Radius) || Radius <= 0.0f)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("radius must be greater than zero"));
    }

    const bool bHDR = GetBoolFieldTextAuth(Params, TEXT("hdr"), false);
    const bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);
    const FLinearColor StartColor = ReadColor(Params, TEXT("startColor"), FLinearColor(0, 0, 0, 1));
    const FLinearColor EndColor = ReadColor(Params, TEXT("endColor"), FLinearColor(1, 1, 1, 1));

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
    const FVector2D GradientDir(FMath::Cos(FMath::DegreesToRadians(Angle)), FMath::Sin(FMath::DegreesToRadians(Angle)));
    for (int32 Y = 0; Y < Height; ++Y)
    {
        for (int32 X = 0; X < Width; ++X)
        {
            const float NX = static_cast<float>(X) / static_cast<float>(Width);
            const float NY = static_cast<float>(Y) / static_cast<float>(Height);
            float T = 0.0f;
            if (GradientType == TEXT("Linear"))
            {
                T = FMath::Clamp(NX * GradientDir.X + NY * GradientDir.Y, 0.0f, 1.0f);
            }
            else if (GradientType == TEXT("Radial"))
            {
                T = FMath::Clamp(FVector2D(NX - CenterX, NY - CenterY).Size() / Radius, 0.0f, 1.0f);
            }
            else if (GradientType == TEXT("Angular"))
            {
                T = FMath::Clamp((FMath::Atan2(NY - CenterY, NX - CenterX) + PI) / (2.0f * PI), 0.0f, 1.0f);
            }

            const FLinearColor Color = FMath::Lerp(StartColor, EndColor, T);
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
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to save gradient texture"));
    }

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Gradient texture '%s' created"), *Name));
    McpHandlerUtils::AddVerification(Response, NewTexture);
    return Response;
}
}

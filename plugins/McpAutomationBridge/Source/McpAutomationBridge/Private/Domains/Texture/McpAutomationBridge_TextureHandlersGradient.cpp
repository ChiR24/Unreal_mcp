#include "Domains/Texture/McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleCreateGradientTexture(const TSharedPtr<FJsonObject>& Params)
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

    const FString GradientType = GetJsonStringField(Params, TEXT("gradientType"), TEXT("Linear"));
    const float Angle = static_cast<float>(GetJsonNumberField(Params, TEXT("angle"), 0.0));
    const float CenterX = static_cast<float>(GetJsonNumberField(Params, TEXT("centerX"), 0.5));
    const float CenterY = static_cast<float>(GetJsonNumberField(Params, TEXT("centerY"), 0.5));
    const float Radius = static_cast<float>(GetJsonNumberField(Params, TEXT("radius"), 0.5));
    if (!FMath::IsFinite(Radius) || Radius <= 0.0f)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("radius must be greater than zero"));
    }

    const bool bHDR = GetJsonBoolField(Params, TEXT("hdr"), false);
    const FLinearColor StartColor = ExtractLinearColorField(Params, TEXT("startColor"), FLinearColor(0, 0, 0, 1));
    const FLinearColor EndColor = ExtractLinearColorField(Params, TEXT("endColor"), FLinearColor(1, 1, 1, 1));


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
    if (!SaveTextureAsset(NewTexture))
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to save gradient texture"));
    }

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Gradient texture '%s' created"), *Name));
    McpHandlerUtils::AddVerification(Response, NewTexture);
    return Response;
}
}

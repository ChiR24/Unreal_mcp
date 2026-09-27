#include "Domains/Texture/McpAutomationBridge_TextureHandlersShared.h"

#include "Math/InterpCurve.h"

namespace McpTextureHandlers
{
// curvePoints ({x, y} in 0-1) is a piecewise-linear curve applied in place to channel.
TSharedPtr<FJsonObject> HandleAdjustCurves(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FInterpCurveFloat Curve;
    const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
    if (Params->TryGetArrayField(TEXT("curvePoints"), Points))
    {
        for (const TSharedPtr<FJsonValue>& Point : *Points)
        {
            const TSharedPtr<FJsonObject>* PointObject = nullptr;
            double X = 0.0;
            double Y = 0.0;
            if (!Point->TryGetObject(PointObject) || !(*PointObject)->TryGetNumberField(TEXT("x"), X) ||
                !(*PointObject)->TryGetNumberField(TEXT("y"), Y))
            {
                TEXTURE_ERROR_RESPONSE(TEXT("Each curvePoints entry must be an object with numeric x and y"));
            }
            Curve.AddPoint(static_cast<float>(X), static_cast<float>(Y));
        }
    }
    if (Curve.Points.Num() < 2)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("curvePoints needs at least two {x, y} points"));
    }

    FString AssetPath;
    FString Error;
    UTexture2D* Texture = LoadSourceTexture(GetJsonStringField(Params, TEXT("assetPath")), TEXT("assetPath"), AssetPath, Error);
    if (!Texture)
    {
        TEXTURE_ERROR_RESPONSE(Error);
    }

    uint8 LUT[256];
    for (int32 i = 0; i < 256; ++i)
    {
        LUT[i] = static_cast<uint8>(FMath::Clamp(Curve.Eval(i / 255.0f, 0.0f) * 255.0f, 0.0f, 255.0f));
    }

    const int32 NumPixels = Texture->Source.GetSizeX() * Texture->Source.GetSizeY();
    const TArray<int32> Channels = ChannelOffsets(GetJsonStringField(Params, TEXT("channel")));
    uint8* MipData = Texture->Source.LockMip(0);
    if (!MipData)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture mip data"));
    }
    for (int32 i = 0; i < NumPixels; ++i)
    {
        for (const int32 c : Channels) MipData[i * 4 + c] = LUT[MipData[i * 4 + c]];
    }
    Texture->Source.UnlockMip(0);
    Texture->UpdateResource();
    Texture->MarkPackageDirty();
    if (GetJsonBoolField(Params, TEXT("save"), true)) McpSafeAssetSave(Texture);

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), TEXT("Curve adjustment applied"));
    Response->SetStringField(TEXT("assetPath"), AssetPath);
    return Response;
}
}

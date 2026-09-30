#include "Domains/Texture/McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleCombineTextures(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    const FString BlendMode = GetJsonStringField(Params, TEXT("blendType"), TEXT("Normal"));
    const float Opacity = FMath::Clamp(static_cast<float>(GetJsonNumberField(Params, TEXT("opacity"), 1.0)), 0.0f, 1.0f);
    FString BasePath;
    FString OverlayPath;
    FString Error;
    UTexture2D* BaseTex = LoadSourceTexture(GetJsonStringField(Params, TEXT("baseTexture")), TEXT("baseTexture"), BasePath, Error);
    UTexture2D* OverlayTex = BaseTex ? LoadSourceTexture(GetJsonStringField(Params, TEXT("blendTexture")), TEXT("blendTexture"), OverlayPath, Error) : nullptr;
    if (!OverlayTex)
    {
        TEXTURE_ERROR_RESPONSE(Error);
    }
    const int32 Width = BaseTex->Source.GetSizeX();
    const int32 Height = BaseTex->Source.GetSizeY();
    if (OverlayTex->Source.GetSizeX() != Width || OverlayTex->Source.GetSizeY() != Height)
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("blendTexture must match baseTexture's %dx%d size"), Width, Height));
    }
    FString Path;
    FString Name;
    if (!ResolveOutputTarget(Params, TEXT("/Game/Textures"), TEXT("Combined"), Path, Name, Error))
    {
        TEXTURE_ERROR_RESPONSE(Error);
    }
    UTexture2D* OutputTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
    if (!OutputTexture)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to create output texture"));
    }
    OutputTexture->SRGB = BaseTex->SRGB;

    const TArray<uint8> BasePixels = ReadSourceBGRA(BaseTex);
    const TArray<uint8> OverlayPixels = ReadSourceBGRA(OverlayTex);
    uint8* OutData = OutputTexture->Source.LockMip(0);
    if (BasePixels.IsEmpty() || OverlayPixels.IsEmpty() || !OutData)
    {
        if (OutData) OutputTexture->Source.UnlockMip(0);
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture data"));
    }
    const uint8* BaseData = BasePixels.GetData();
    const uint8* OverlayData = OverlayPixels.GetData();

    for (int32 Index = 0; Index < Width * Height; ++Index)
    {
        const int32 Idx = Index * 4;
        for (int32 Channel = 0; Channel < 3; ++Channel)
        {
            const float Base = BaseData[Idx + Channel] / 255.0f;
            const float Overlay = OverlayData[Idx + Channel] / 255.0f;
            float Result = Overlay;
            if (BlendMode.Equals(TEXT("Multiply"), ESearchCase::IgnoreCase))
            {
                Result = Base * Overlay;
            }
            else if (BlendMode.Equals(TEXT("Screen"), ESearchCase::IgnoreCase))
            {
                Result = 1.0f - (1.0f - Base) * (1.0f - Overlay);
            }
            else if (BlendMode.Equals(TEXT("Overlay"), ESearchCase::IgnoreCase))
            {
                Result = Base < 0.5f ? 2.0f * Base * Overlay : 1.0f - 2.0f * (1.0f - Base) * (1.0f - Overlay);
            }
            else if (BlendMode.Equals(TEXT("Add"), ESearchCase::IgnoreCase))
            {
                Result = FMath::Min(Base + Overlay, 1.0f);
            }
            OutData[Idx + Channel] = static_cast<uint8>(FMath::Clamp(FMath::Lerp(Base, Result, Opacity) * 255.0f, 0.0f, 255.0f));
        }
        OutData[Idx + 3] = BaseData[Idx + 3];
    }

    OutputTexture->Source.UnlockMip(0);
    OutputTexture->UpdateResource();
    FAssetRegistryModule::AssetCreated(OutputTexture);
    McpSafeAssetSave(OutputTexture);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Textures combined (mode: %s)"), *BlendMode));
    Response->SetStringField(TEXT("assetPath"), Path / Name);
    return Response;
}
}

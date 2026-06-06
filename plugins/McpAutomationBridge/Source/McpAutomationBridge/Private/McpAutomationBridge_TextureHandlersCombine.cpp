#include "McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleCombineTextures(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString BaseTexturePath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("baseTexture"), TEXT("")));
    FString OverlayTexturePath = NormalizeTexturePath(GetStringFieldTextAuth(
        Params, TEXT("overlayTexture"), GetStringFieldTextAuth(Params, TEXT("blendTexture"), TEXT(""))));
    const FString BlendMode = GetStringFieldTextAuth(Params, TEXT("blendMode"), TEXT("Normal"));
    const float Opacity = FMath::Clamp(static_cast<float>(GetNumberFieldTextAuth(Params, TEXT("opacity"), 1.0)), 0.0f, 1.0f);
    FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT("Combined"));
    FString Path = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures")));
    const bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);

    if (BaseTexturePath.IsEmpty() || OverlayTexturePath.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("baseTexture and overlayTexture are required"));
    }
    const FString SanitizedBase = SanitizeProjectRelativePath(BaseTexturePath);
    const FString SanitizedOverlay = SanitizeProjectRelativePath(OverlayTexturePath);
    if (SanitizedBase.IsEmpty() || SanitizedOverlay.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid baseTexture or overlayTexture path: contains traversal or invalid characters"));
    }
    BaseTexturePath = SanitizedBase;
    OverlayTexturePath = SanitizedOverlay;

    UTexture2D* BaseTex = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *BaseTexturePath));
    UTexture2D* OverlayTex = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *OverlayTexturePath));
    if (!BaseTex || !OverlayTex)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to load base or overlay texture"));
    }

    const int32 Width = BaseTex->GetSizeX();
    const int32 Height = BaseTex->GetSizeY();
    UTexture2D* OutputTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
    if (!OutputTexture)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to create output texture"));
    }
    if (!BaseTex->Source.IsValid())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Base texture has no source data - may be compressed or not fully loaded"));
    }
    if (!OverlayTex->Source.IsValid())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Overlay texture has no source data - may be compressed or not fully loaded"));
    }
    if (BaseTex->IsStreamable()) BaseTex->SetForceMipLevelsToBeResident(30.0f);
    if (OverlayTex->IsStreamable()) OverlayTex->SetForceMipLevelsToBeResident(30.0f);

    const uint8* BaseData = BaseTex->Source.LockMipReadOnly(0);
    const uint8* OverlayData = OverlayTex->Source.LockMipReadOnly(0);
    uint8* OutData = OutputTexture->Source.LockMip(0);
    if (!BaseData || !OverlayData || !OutData)
    {
        if (BaseData) BaseTex->Source.UnlockMip(0);
        if (OverlayData) OverlayTex->Source.UnlockMip(0);
        if (OutData) OutputTexture->Source.UnlockMip(0);
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture data"));
    }

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

    BaseTex->Source.UnlockMip(0);
    OverlayTex->Source.UnlockMip(0);
    OutputTexture->Source.UnlockMip(0);
    OutputTexture->UpdateResource();
    if (bSave)
    {
        FAssetRegistryModule::AssetCreated(OutputTexture);
        McpSafeAssetSave(OutputTexture);
    }
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Textures combined (mode: %s)"), *BlendMode));
    Response->SetStringField(TEXT("assetPath"), Path / Name);
    return Response;
}
}

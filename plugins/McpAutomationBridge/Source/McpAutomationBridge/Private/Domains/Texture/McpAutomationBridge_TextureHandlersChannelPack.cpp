#include "Domains/Texture/McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleChannelPack(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    const TCHAR* const Fields[] = { TEXT("redTexture"), TEXT("greenTexture"), TEXT("blueTexture"), TEXT("alphaTexture") };
    const int32 Offsets[] = { 2, 1, 0, 3 };
    UTexture2D* Sources[4] = {};
    int32 Width = 0;
    int32 Height = 0;
    FString Error;
    FString SourcePath;
    for (int32 i = 0; i < 4; ++i)
    {
        const FString Raw = GetJsonStringField(Params, Fields[i]);
        if (Raw.IsEmpty()) continue;
        Sources[i] = LoadSourceTexture(Raw, Fields[i], SourcePath, Error);
        if (!Sources[i])
        {
            TEXTURE_ERROR_RESPONSE(Error);
        }
        if (Width == 0)
        {
            Width = Sources[i]->Source.GetSizeX();
            Height = Sources[i]->Source.GetSizeY();
        }
    }
    if (Width == 0)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("At least one source texture (redTexture, greenTexture, blueTexture, or alphaTexture) is required"));
    }

    FString Path;
    FString Name;
    if (!ResolveOutputTarget(Params, TEXT("/Game/Textures"), TEXT("ChannelPacked"), Path, Name, Error))
    {
        TEXTURE_ERROR_RESPONSE(Error);
    }
    UTexture2D* OutputTexture = CreateEmptyTexture(Path, Name, Width, Height, false);
    if (!OutputTexture)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to create output texture"));
    }
    OutputTexture->PreEditChange(nullptr);
    OutputTexture->SRGB = false;
    OutputTexture->CompressionSettings = TC_Masks;
    OutputTexture->PostEditChange();

    uint8* OutData = OutputTexture->Source.LockMip(0);
    if (!OutData)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock output texture data"));
    }
    // A missing or smaller source leaves its channel 0 (alpha 255). A channel copies the same channel of its
    // source; a G8 source is one gray channel, so its gray feeds whichever channel it is assigned, alpha included.
    for (int32 i = 0; i < 4; ++i)
    {
        const TArray<uint8> Src = Sources[i] ? ReadSourceBGRA(Sources[i]) : TArray<uint8>();
        const int32 SrcPixels = Src.Num() / 4;
        const int32 SrcOffset = Sources[i] && Sources[i]->Source.GetFormat() == TSF_G8 ? 0 : Offsets[i];
        for (int32 Pixel = 0; Pixel < Width * Height; ++Pixel)
        {
            OutData[Pixel * 4 + Offsets[i]] = Pixel < SrcPixels ? Src[Pixel * 4 + SrcOffset] : (i == 3 ? 255 : 0);
        }
    }

    OutputTexture->Source.UnlockMip(0);
    OutputTexture->UpdateResource();
    FAssetRegistryModule::AssetCreated(OutputTexture);
    McpSafeAssetSave(OutputTexture);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), TEXT("Channels packed into single texture"));
    Response->SetStringField(TEXT("assetPath"), Path / Name);
    return Response;
}
}

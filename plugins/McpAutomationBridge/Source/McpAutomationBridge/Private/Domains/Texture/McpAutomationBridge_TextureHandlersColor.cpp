#include "Domains/Texture/McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
// invert, desaturate and adjust_levels edit the texture in place.
TSharedPtr<FJsonObject> HandleTextureColorAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString AssetPath;
    FString Error;
    UTexture2D* Texture = LoadSourceTexture(GetJsonStringField(Params, TEXT("assetPath")), TEXT("assetPath"), AssetPath, Error, /*bConvertToBGRA8=*/true);
    if (!Texture)
    {
        TEXTURE_ERROR_RESPONSE(Error);
    }

    const int32 NumPixels = Texture->Source.GetSizeX() * Texture->Source.GetSizeY();
    const TArray<int32> Channels = ChannelOffsets(GetJsonStringField(Params, TEXT("channel")));
    uint8* MipData = Texture->Source.LockMip(0);
    if (!MipData)
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Failed to lock texture mip data"));
    }

    if (SubAction == TEXT("invert"))
    {
        for (int32 i = 0; i < NumPixels; ++i)
        {
            for (const int32 c : Channels) MipData[i * 4 + c] = 255 - MipData[i * 4 + c];
        }
        Response->SetStringField(TEXT("message"), TEXT("Texture colors inverted"));
    }
    else if (SubAction == TEXT("desaturate"))
    {
        const float Amount = FMath::Clamp(static_cast<float>(GetJsonNumberField(Params, TEXT("amount"), 1.0)), 0.0f, 1.0f);
        for (int32 i = 0; i < NumPixels; ++i)
        {
            uint8* Pixel = MipData + i * 4;
            const float Gray = 0.2126f * Pixel[2] + 0.7152f * Pixel[1] + 0.0722f * Pixel[0];
            for (int32 c = 0; c < 3; ++c) Pixel[c] = static_cast<uint8>(FMath::Lerp(static_cast<float>(Pixel[c]), Gray, Amount));
        }
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Texture desaturated (amount: %.2f)"), Amount));
    }
    else
    {
        const float InBlack = FMath::Clamp(static_cast<float>(GetJsonNumberField(Params, TEXT("inBlack"), 0.0)), 0.0f, 1.0f);
        const float InWhite = FMath::Clamp(static_cast<float>(GetJsonNumberField(Params, TEXT("inWhite"), 1.0)), 0.0f, 1.0f);
        const float InvGamma = 1.0f / FMath::Max(static_cast<float>(GetJsonNumberField(Params, TEXT("gamma"), 1.0)), 0.01f);
        const float InRange = FMath::Max(InWhite - InBlack, 0.001f);
        for (int32 i = 0; i < NumPixels; ++i)
        {
            for (const int32 c : Channels)
            {
                const float Val = FMath::Clamp((MipData[i * 4 + c] / 255.0f - InBlack) / InRange, 0.0f, 1.0f);
                MipData[i * 4 + c] = static_cast<uint8>(FMath::Clamp(FMath::Pow(Val, InvGamma) * 255.0f, 0.0f, 255.0f));
            }
        }
        Response->SetStringField(TEXT("message"), TEXT("Levels adjusted"));
    }

    Texture->Source.UnlockMip(0);
    Texture->UpdateResource();
    Texture->MarkPackageDirty();
    if (GetJsonBoolField(Params, TEXT("save"), true)) McpSafeAssetSave(Texture);
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("assetPath"), AssetPath);
    return Response;
}
}

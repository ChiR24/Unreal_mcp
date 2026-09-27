#include "Domains/Texture/McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleTextureInfoAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params)
{
    if (SubAction != TEXT("get_texture_info"))
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString AssetPath = GetJsonStringField(Params, TEXT("assetPath"), TEXT(""));
    const FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (SanitizedAssetPath.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("Invalid assetPath: contains traversal or invalid characters"));
    }
    AssetPath = SanitizedAssetPath;
    if (AssetPath.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
    }

    UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
    if (!Texture)
    {
        TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> TextureInfo = McpHandlerUtils::CreateResultObject();
    TextureInfo->SetNumberField(TEXT("width"), Texture->GetSizeX());
    TextureInfo->SetNumberField(TEXT("height"), Texture->GetSizeY());
    TextureInfo->SetStringField(TEXT("format"), GPixelFormats[Texture->GetPixelFormat()].Name);
    TextureInfo->SetNumberField(TEXT("mipCount"), Texture->GetNumMips());
    TextureInfo->SetBoolField(TEXT("sRGB"), Texture->SRGB);
    TextureInfo->SetBoolField(TEXT("virtualTextureStreaming"), Texture->VirtualTextureStreaming);
    TextureInfo->SetBoolField(TEXT("neverStream"), Texture->NeverStream);
    TextureInfo->SetNumberField(TEXT("lodBias"), Texture->LODBias);
    TextureInfo->SetStringField(TEXT("compression"), StaticEnum<TextureCompressionSettings>()->GetNameStringByValue(Texture->CompressionSettings));

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), TEXT("Texture info retrieved"));
    Response->SetObjectField(TEXT("textureInfo"), TextureInfo);
    return Response;
}
}

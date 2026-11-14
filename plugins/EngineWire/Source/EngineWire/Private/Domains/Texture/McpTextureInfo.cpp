#include "Domains/Texture/McpTextureShared.h"
#include "Foundation/BridgeHelpers/Reflection/McpEnumName.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleTextureInfoAction(const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString AssetPath = GetJsonStringField(Params, TEXT("assetPath"), TEXT(""));
    const FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (SanitizedAssetPath.IsEmpty())
    {
        TEXTURE_ERROR_RESPONSE(McpPathRefusalMessage(TEXT("assetPath"), AssetPath));
    }
    FString Error;
    UTexture2D* Texture = LoadTextureAt(SanitizedAssetPath, TEXT("assetPath"), AssetPath, Error);
    if (!Texture)
    {
        TEXTURE_ERROR_RESPONSE(Error);
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
    TextureInfo->SetStringField(TEXT("compression"), McpEnumName(Texture->CompressionSettings));

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), TEXT("Texture info retrieved"));
    Response->SetObjectField(TEXT("textureInfo"), TextureInfo);
    return Response;
}
}

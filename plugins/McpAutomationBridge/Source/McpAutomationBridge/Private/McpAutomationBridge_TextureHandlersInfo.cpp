#include "McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
namespace
{
FString CompressionToString(TextureCompressionSettings Compression)
{
    switch (Compression)
    {
        case TC_Default: return TEXT("TC_Default");
        case TC_Normalmap: return TEXT("TC_Normalmap");
        case TC_Masks: return TEXT("TC_Masks");
        case TC_Grayscale: return TEXT("TC_Grayscale");
        case TC_Displacementmap: return TEXT("TC_Displacementmap");
        case TC_VectorDisplacementmap: return TEXT("TC_VectorDisplacementmap");
        case TC_HDR: return TEXT("TC_HDR");
        case TC_EditorIcon: return TEXT("TC_EditorIcon");
        case TC_Alpha: return TEXT("TC_Alpha");
        case TC_DistanceFieldFont: return TEXT("TC_DistanceFieldFont");
        case TC_HDR_Compressed: return TEXT("TC_HDR_Compressed");
        case TC_BC7: return TEXT("TC_BC7");
        default: return TEXT("Unknown");
    }
}
}

TSharedPtr<FJsonObject> HandleTextureInfoAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params)
{
    if (SubAction != TEXT("get_texture_info"))
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString AssetPath = GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT(""));
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
    TextureInfo->SetStringField(TEXT("compression"), CompressionToString(Texture->CompressionSettings));

    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("message"), TEXT("Texture info retrieved"));
    Response->SetObjectField(TEXT("textureInfo"), TextureInfo);
    return Response;
}
}

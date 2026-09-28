#include "Domains/Texture/McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
namespace
{
UTexture2D* LoadTextureChecked(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject>& Response, FString& AssetPath)
{
    AssetPath = GetJsonStringField(Params, TEXT("assetPath"), TEXT(""));
    const FString SanitizedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (SanitizedAssetPath.IsEmpty())
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("Invalid assetPath: contains traversal or invalid characters"));
        return nullptr;
    }
    AssetPath = SanitizedAssetPath;
    if (AssetPath.IsEmpty())
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("assetPath is required"));
        return nullptr;
    }
    UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
    if (!Texture)
    {
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        return nullptr;
    }
    return Texture;
}

TextureCompressionSettings ParseCompressionSetting(const FString& Value)
{
    // Any engine TextureCompressionSettings name; TC_Default otherwise.
    const int64 Compression = StaticEnum<TextureCompressionSettings>()->GetValueByNameString(Value);
    return Compression == INDEX_NONE ? TC_Default : static_cast<TextureCompressionSettings>(Compression);
}

::TextureGroup ParseTextureGroup(const FString& Value)
{
    if (Value.Contains(TEXT("Character"))) return TEXTUREGROUP_Character;
    if (Value.Contains(TEXT("Weapon"))) return TEXTUREGROUP_Weapon;
    if (Value.Contains(TEXT("Vehicle"))) return TEXTUREGROUP_Vehicle;
    if (Value.Contains(TEXT("Cinematic"))) return TEXTUREGROUP_Cinematic;
    if (Value.Contains(TEXT("Effects"))) return TEXTUREGROUP_Effects;
    if (Value.Contains(TEXT("Skybox"))) return TEXTUREGROUP_Skybox;
    if (Value.Contains(TEXT("UI"))) return TEXTUREGROUP_UI;
    if (Value.Contains(TEXT("Lightmap"))) return TEXTUREGROUP_Lightmap;
    if (Value.Contains(TEXT("RenderTarget"))) return TEXTUREGROUP_RenderTarget;
    if (Value.Contains(TEXT("Bokeh"))) return TEXTUREGROUP_Bokeh;
    if (Value.Contains(TEXT("Pixels2D"))) return TEXTUREGROUP_Pixels2D;
    return TEXTUREGROUP_World;
}
}

TSharedPtr<FJsonObject> HandleTextureSettingsAction(const FString& SubAction, const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    FString AssetPath;
    UTexture2D* Texture = LoadTextureChecked(Params, Response, AssetPath);
    if (!Texture)
    {
        return Response;
    }
    const bool bSave = GetJsonBoolField(Params, TEXT("save"), true);

    if (SubAction == TEXT("set_compression_settings"))
    {
        const FString CompressionSettingsStr = GetJsonStringField(Params, TEXT("compressionSettings"), TEXT("TC_Default"));
        Texture->PreEditChange(nullptr);
        Texture->CompressionSettings = ParseCompressionSetting(CompressionSettingsStr);
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        if (bSave) McpSafeAssetSave(Texture);
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Compression set to %s"), *CompressionSettingsStr));
        McpHandlerUtils::AddVerification(Response, Texture);
        return Response;
    }

    if (SubAction == TEXT("set_texture_group"))
    {
        const FString TextureGroup = GetJsonStringField(Params, TEXT("textureGroup"), TEXT("TEXTUREGROUP_World"));
        Texture->PreEditChange(nullptr);
        Texture->LODGroup = ParseTextureGroup(TextureGroup);
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        if (bSave) McpSafeAssetSave(Texture);
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Texture group set to %s"), *TextureGroup));
        McpHandlerUtils::AddVerification(Response, Texture);
        return Response;
    }

    if (SubAction == TEXT("set_lod_bias"))
    {
        const int32 LODBias = static_cast<int32>(GetJsonNumberField(Params, TEXT("lodBias"), 0));
        Texture->PreEditChange(nullptr);
        Texture->LODBias = LODBias;
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        if (bSave) McpSafeAssetSave(Texture);
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("LOD bias set to %d"), LODBias));
        McpHandlerUtils::AddVerification(Response, Texture);
        return Response;
    }

    if (SubAction == TEXT("configure_virtual_texture"))
    {
        const bool bVirtualTextureStreaming = GetJsonBoolField(Params, TEXT("virtualTextureStreaming"), false);
        Texture->PreEditChange(nullptr);
        Texture->VirtualTextureStreaming = bVirtualTextureStreaming;
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        if (bSave) McpSafeAssetSave(Texture);
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Virtual texture streaming %s"), bVirtualTextureStreaming ? TEXT("enabled") : TEXT("disabled")));
        Response->SetStringField(TEXT("assetPath"), AssetPath);
        Response->SetBoolField(TEXT("virtualTextureStreaming"), bVirtualTextureStreaming);
        return Response;
    }

    if (SubAction == TEXT("set_streaming_priority"))
    {
        const bool bNeverStream = GetJsonBoolField(Params, TEXT("neverStream"), false);
        Texture->PreEditChange(nullptr);
        Texture->NeverStream = bNeverStream;
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        if (bSave) McpSafeAssetSave(Texture);
        Response->SetBoolField(TEXT("success"), true);
        // Only neverStream is applied: UTexture has no per-texture streaming
        // priority across 5.0-5.8, so the reply no longer claims one was set.
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("NeverStream set to %s"), bNeverStream ? TEXT("true") : TEXT("false")));
        Response->SetBoolField(TEXT("neverStream"), bNeverStream);
        McpHandlerUtils::AddVerification(Response, Texture);
        return Response;
    }

    return nullptr;
}
}

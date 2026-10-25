#include "Domains/Texture/McpTextureShared.h"

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
        Response->SetStringField(TEXT("error"), McpPathRefusalMessage(TEXT("assetPath"), AssetPath));
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

// An engine enum value by its name, with or without the prefix (TC_Normalmap or Normalmap). An unknown compression name
// became TC_Default and a group was matched by substring ("Building" landed in UI), each answering success.
template <typename TEnum>
bool ParseEngineEnum(const FString& Value, const TCHAR* Prefix, TEnum& Out)
{
    const UEnum* Enum = StaticEnum<TEnum>();
    int64 Found = Value.IsEmpty() ? INDEX_NONE : Enum->GetValueByNameString(Value);
    Found = Found == INDEX_NONE && !Value.IsEmpty() ? Enum->GetValueByNameString(Prefix + Value) : Found;
    if (Found == INDEX_NONE || Found >= Enum->GetMaxEnumValue())
    {
        return false;
    }
    Out = static_cast<TEnum>(Found);
    return true;
}

// The names a refusal offers, hidden entries left out and Prefix cut off: every group with its TEXTUREGROUP_ ran past
// what a refusal message carries.
template <typename TEnum>
TArray<FString> EngineEnumNames(const TCHAR* Prefix)
{
    const UEnum* Enum = StaticEnum<TEnum>();
    TArray<FString> Names;
    for (int32 Index = 0; Index < Enum->NumEnums() - 1; ++Index)
    {
        if (!Enum->HasMetaData(TEXT("Hidden"), Index))
        {
            Names.Add(Enum->GetNameStringByIndex(Index).RightChop(FCString::Strlen(Prefix)));
        }
    }
    return Names;
}

TSharedPtr<FJsonObject> Refuse(const TSharedPtr<FJsonObject>& Response, const FString& Error, const TArray<FString>& ValidValues = {})
{
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), Error);
    Response->SetStringField(TEXT("errorCode"), TEXT("INVALID_ARGUMENT"));
    if (ValidValues.Num() > 0)
    {
        Response->SetArrayField(TEXT("validValues"), McpHandlerUtils::ToJsonStringArray(ValidValues));
    }
    return Response;
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
        const FString CompressionSettingsStr = GetJsonStringField(Params, TEXT("compressionSettings"));
        TextureCompressionSettings Compression = TC_Default;
        if (!ParseEngineEnum(CompressionSettingsStr, TEXT("TC_"), Compression))
        {
            const TArray<FString> Valid = EngineEnumNames<TextureCompressionSettings>(TEXT("TC_"));
            return Refuse(Response, FString::Printf(TEXT("compressionSettings '%s' is not a texture compression setting; use one of %s "
                "(the TC_ prefix is optional). Nothing was changed."), *CompressionSettingsStr, *FString::Join(Valid, TEXT(", "))), Valid);
        }
        Texture->PreEditChange(nullptr);
        Texture->CompressionSettings = Compression;
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        Response->SetBoolField(TEXT("saved"), bSave && McpSafeAssetSave(Texture));
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("compressionSettings"), StaticEnum<TextureCompressionSettings>()->GetNameStringByValue(Compression));
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Compression set to %s"), *StaticEnum<TextureCompressionSettings>()->GetNameStringByValue(Compression)));
        McpHandlerUtils::AddVerification(Response, Texture);
        return Response;
    }

    if (SubAction == TEXT("set_texture_group"))
    {
        const FString TextureGroup = GetJsonStringField(Params, TEXT("textureGroup"));
        ::TextureGroup Group = TEXTUREGROUP_World;
        if (!ParseEngineEnum(TextureGroup, TEXT("TEXTUREGROUP_"), Group))
        {
            return Refuse(Response, FString::Printf(TEXT("textureGroup '%s' is not a texture group; use an engine group such as World, "
                "WorldNormalMap, Character, Weapon, Vehicle, Cinematic, Effects, Skybox, UI, Lightmap, Shadowmap, Pixels2D or "
                "Project01 to Project32, the TEXTUREGROUP_ prefix optional. Nothing was changed."), *TextureGroup),
                EngineEnumNames<::TextureGroup>(TEXT("TEXTUREGROUP_")));
        }
        Texture->PreEditChange(nullptr);
        Texture->LODGroup = Group;
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        Response->SetBoolField(TEXT("saved"), bSave && McpSafeAssetSave(Texture));
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("textureGroup"), StaticEnum<::TextureGroup>()->GetNameStringByValue(Group));
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Texture group set to %s"), *StaticEnum<::TextureGroup>()->GetNameStringByValue(Group)));
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
        Response->SetBoolField(TEXT("saved"), bSave && McpSafeAssetSave(Texture));
        Response->SetBoolField(TEXT("success"), true);
        Response->SetNumberField(TEXT("lodBias"), LODBias);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("LOD bias set to %d"), LODBias));
        McpHandlerUtils::AddVerification(Response, Texture);
        return Response;
    }

    if (SubAction == TEXT("configure_virtual_texture"))
    {
        // Left out, this switched streaming off.
        if (!Params->HasField(TEXT("virtualTextureStreaming")))
        {
            return Refuse(Response, TEXT("Send virtualTextureStreaming: true to stream the texture as a virtual texture, false to stop. Nothing was changed."));
        }
        const bool bVirtualTextureStreaming = GetJsonBoolField(Params, TEXT("virtualTextureStreaming"), false);
        Texture->PreEditChange(nullptr);
        Texture->VirtualTextureStreaming = bVirtualTextureStreaming;
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        Response->SetBoolField(TEXT("saved"), bSave && McpSafeAssetSave(Texture));
        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Virtual texture streaming %s"), bVirtualTextureStreaming ? TEXT("enabled") : TEXT("disabled")));
        Response->SetStringField(TEXT("assetPath"), AssetPath);
        Response->SetBoolField(TEXT("virtualTextureStreaming"), bVirtualTextureStreaming);
        return Response;
    }

    if (SubAction == TEXT("set_streaming_priority"))
    {
        // Left out, this switched never-stream off.
        if (!Params->HasField(TEXT("neverStream")))
        {
            return Refuse(Response, TEXT("Send neverStream: true to keep every mip resident, false to let it stream. Nothing was changed."));
        }
        const bool bNeverStream = GetJsonBoolField(Params, TEXT("neverStream"), false);
        Texture->PreEditChange(nullptr);
        Texture->NeverStream = bNeverStream;
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();
        Response->SetBoolField(TEXT("saved"), bSave && McpSafeAssetSave(Texture));
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

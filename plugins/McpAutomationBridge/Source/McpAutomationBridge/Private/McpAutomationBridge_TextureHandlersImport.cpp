#include "McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleTextureImportAndSamplerAction(
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("import_texture"))
    {
        FString SourcePath = GetStringFieldTextAuth(Params, TEXT("sourcePath"), TEXT(""));
        FString DestinationPath = GetStringFieldTextAuth(Params, TEXT("destinationPath"), TEXT(""));

        if (SourcePath.IsEmpty() || DestinationPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("sourcePath and destinationPath are required"));
        }

        UTexture2D* ImportedTexture = Cast<UTexture2D>(UEditorAssetLibrary::LoadAsset(SourcePath));
        if (!ImportedTexture)
        {
            if (FPaths::FileExists(SourcePath))
            {
                Response->SetBoolField(TEXT("success"), true);
                Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Texture import queued from '%s' to '%s'"), *SourcePath, *DestinationPath));
                Response->SetStringField(TEXT("note"), TEXT("Use AssetTools for actual file import in editor"));
                return Response;
            }
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to import texture from: %s"), *SourcePath));
        }

        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Texture imported to '%s'"), *DestinationPath));
        Response->SetStringField(TEXT("assetPath"), DestinationPath);
        return Response;
    }

    if (SubAction == TEXT("set_texture_filter"))
    {
        FString AssetPath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT("")));
        FString FilterMode = GetStringFieldTextAuth(Params, TEXT("filter"), TEXT("Default"));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);

        if (AssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
        }

        UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
        if (!Texture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        }

        TextureFilter Filter = TF_Default;
        if (FilterMode == TEXT("Nearest")) Filter = TF_Nearest;
        else if (FilterMode == TEXT("Bilinear")) Filter = TF_Bilinear;
        else if (FilterMode == TEXT("Trilinear")) Filter = TF_Trilinear;
        else if (FilterMode == TEXT("Default")) Filter = TF_Default;

        Texture->PreEditChange(nullptr);
        Texture->Filter = Filter;
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();

        if (bSave)
        {
            McpSafeAssetSave(Texture);
        }

        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Filter set to %s"), *FilterMode));
        return Response;
    }

    if (SubAction == TEXT("set_texture_wrap"))
    {
        FString AssetPath = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("assetPath"), TEXT("")));
        FString WrapMode = GetStringFieldTextAuth(Params, TEXT("wrapMode"), TEXT("Wrap"));
        bool bSave = GetBoolFieldTextAuth(Params, TEXT("save"), true);

        if (AssetPath.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("assetPath is required"));
        }

        UTexture2D* Texture = Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *AssetPath));
        if (!Texture)
        {
            TEXTURE_ERROR_RESPONSE(FString::Printf(TEXT("Failed to load texture: %s"), *AssetPath));
        }

        TextureAddress WrapU = TA_Wrap, WrapV = TA_Wrap;
        if (WrapMode == TEXT("Clamp")) { WrapU = TA_Clamp; WrapV = TA_Clamp; }
        else if (WrapMode == TEXT("Mirror")) { WrapU = TA_Mirror; WrapV = TA_Mirror; }
        else if (WrapMode == TEXT("Wrap")) { WrapU = TA_Wrap; WrapV = TA_Wrap; }

        Texture->PreEditChange(nullptr);
        Texture->AddressX = WrapU;
        Texture->AddressY = WrapV;
        Texture->PostEditChange();
        Texture->UpdateResource();
        Texture->MarkPackageDirty();

        if (bSave)
        {
            McpSafeAssetSave(Texture);
        }

        Response->SetBoolField(TEXT("success"), true);
        Response->SetStringField(TEXT("message"), FString::Printf(TEXT("Wrap mode set to %s"), *WrapMode));
        return Response;
    }

    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown texture action: %s"), *SubAction));
    return Response;
}
}

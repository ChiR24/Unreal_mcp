#include "McpAutomationBridge_TextureHandlersShared.h"

namespace McpTextureHandlers
{
TSharedPtr<FJsonObject> HandleTexturePlaceholderAction(
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Params)
{
    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("create_cube_texture"))
    {
        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures")));
        int32 Size = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("size"), 512));
        if (Name.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("name is required"));
        }
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("UNSUPPORTED_OPERATION"));
        Response->SetStringField(TEXT("errorCode"), TEXT("UNSUPPORTED_OPERATION"));
        Response->SetStringField(TEXT("message"), TEXT("create_cube_texture is not implemented for generated assets. Import a real cube map source with import_texture instead."));
        return Response;
    }

    if (SubAction == TEXT("create_volume_texture"))
    {
        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures")));
        int32 Width = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("width"), 256));
        int32 Height = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("height"), 256));
        int32 Depth = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("depth"), 256));
        if (Name.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("name is required"));
        }
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("UNSUPPORTED_OPERATION"));
        Response->SetStringField(TEXT("errorCode"), TEXT("UNSUPPORTED_OPERATION"));
        Response->SetStringField(TEXT("message"), TEXT("create_volume_texture is not implemented for generated assets. Import a real volume texture source instead."));
        return Response;
    }

    if (SubAction == TEXT("create_texture_array"))
    {
        FString Name = GetStringFieldTextAuth(Params, TEXT("name"), TEXT(""));
        FString Path = NormalizeTexturePath(GetStringFieldTextAuth(Params, TEXT("path"), TEXT("/Game/Textures")));
        int32 Width = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("width"), 512));
        int32 Height = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("height"), 512));
        int32 NumSlices = static_cast<int32>(GetNumberFieldTextAuth(Params, TEXT("numSlices"), 4));
        if (Name.IsEmpty())
        {
            TEXTURE_ERROR_RESPONSE(TEXT("name is required"));
        }
        Response->SetBoolField(TEXT("success"), false);
        Response->SetStringField(TEXT("error"), TEXT("UNSUPPORTED_OPERATION"));
        Response->SetStringField(TEXT("errorCode"), TEXT("UNSUPPORTED_OPERATION"));
        Response->SetStringField(TEXT("message"), TEXT("create_texture_array is not implemented for generated assets. Import or assemble real texture slices instead."));
        return Response;
    }

    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown texture action: %s"), *SubAction));
    return Response;
}
}

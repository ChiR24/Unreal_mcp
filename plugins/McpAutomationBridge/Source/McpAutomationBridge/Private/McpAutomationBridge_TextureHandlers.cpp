#include "McpAutomationBridge_TextureHandlersShared.h"

TSharedPtr<FJsonObject> UMcpAutomationBridgeSubsystem::HandleManageTextureAction(const TSharedPtr<FJsonObject>& Params)
{
    const FString SubAction = McpTextureHandlers::GetStringField(Params, TEXT("subAction"), TEXT(""));
    if (SubAction == TEXT("create_noise_texture"))
    {
        return McpTextureHandlers::HandleCreateNoiseTexture(Params);
    }
    if (SubAction == TEXT("create_gradient_texture"))
    {
        return McpTextureHandlers::HandleCreateGradientTexture(Params);
    }
    if (SubAction == TEXT("create_pattern_texture"))
    {
        return McpTextureHandlers::HandleCreatePatternTexture(Params);
    }
    if (SubAction == TEXT("create_normal_from_height"))
    {
        return McpTextureHandlers::HandleCreateNormalFromHeight(Params);
    }
    if (SubAction == TEXT("set_compression_settings") || SubAction == TEXT("set_texture_group") ||
        SubAction == TEXT("set_lod_bias") || SubAction == TEXT("configure_virtual_texture") ||
        SubAction == TEXT("set_streaming_priority"))
    {
        return McpTextureHandlers::HandleTextureSettingsAction(SubAction, Params);
    }
    if (SubAction == TEXT("get_texture_info"))
    {
        return McpTextureHandlers::HandleTextureInfoAction(SubAction, Params);
    }
    if (SubAction == TEXT("resize_texture"))
    {
        return McpTextureHandlers::HandleResizeTexture(Params);
    }
    if (SubAction == TEXT("invert") || SubAction == TEXT("desaturate") || SubAction == TEXT("adjust_levels"))
    {
        return McpTextureHandlers::HandleTextureColorAction(SubAction, Params);
    }
    if (SubAction == TEXT("blur") || SubAction == TEXT("sharpen"))
    {
        return McpTextureHandlers::HandleTextureFilterAction(SubAction, Params);
    }
    if (SubAction == TEXT("channel_pack"))
    {
        return McpTextureHandlers::HandleChannelPack(Params);
    }
    if (SubAction == TEXT("combine_textures"))
    {
        return McpTextureHandlers::HandleCombineTextures(Params);
    }
    if (SubAction == TEXT("adjust_curves"))
    {
        return McpTextureHandlers::HandleAdjustCurves(Params);
    }
    if (SubAction == TEXT("channel_extract"))
    {
        return McpTextureHandlers::HandleChannelExtract(Params);
    }
    if (SubAction == TEXT("import_texture") || SubAction == TEXT("set_texture_filter") ||
        SubAction == TEXT("set_texture_wrap"))
    {
        return McpTextureHandlers::HandleTextureImportAndSamplerAction(SubAction, Params);
    }
    if (SubAction == TEXT("create_render_target"))
    {
        return McpTextureHandlers::HandleCreateRenderTarget(Params);
    }
    if (SubAction == TEXT("create_cube_texture") || SubAction == TEXT("create_volume_texture") ||
        SubAction == TEXT("create_texture_array"))
    {
        return McpTextureHandlers::HandleTexturePlaceholderAction(SubAction, Params);
    }
    if (SubAction == TEXT("create_ao_from_mesh"))
    {
        return McpTextureHandlers::HandleCreateAoFromMesh(Params);
    }

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown texture action: %s"), *SubAction));
    return Response;
}

bool UMcpAutomationBridgeSubsystem::HandleManageTextureAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    if (Action != TEXT("manage_texture"))
    {
        return false;
    }

    const TSharedPtr<FJsonObject> Result = HandleManageTextureAction(Payload);
    if (!Result.IsValid())
    {
        SendAutomationError(
            Socket, RequestId, TEXT("Failed to process texture action"), TEXT("PROCESSING_FAILED"));
        return true;
    }

    const bool bSuccess = GetJsonBoolField(Result, TEXT("success"));
    if (bSuccess)
    {
        const FString Message = GetJsonStringField(Result, TEXT("message"));
        SendAutomationResponse(Socket, RequestId, true, Message, Result);
        return true;
    }

    const FString Error = GetJsonStringField(Result, TEXT("error"), TEXT("Unknown error"));
    const FString ErrorCode = GetJsonStringField(Result, TEXT("errorCode"), TEXT("TEXTURE_ERROR"));
    SendAutomationError(Socket, RequestId, Error, ErrorCode);
    return true;
}

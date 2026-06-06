#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleAddTextureSample(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_texture_sample")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString TexturePath, ParameterName, SamplerType;
    Payload->TryGetStringField(TEXT("texturePath"), TexturePath);
    Payload->TryGetStringField(TEXT("parameterName"), ParameterName);
    Payload->TryGetStringField(TEXT("samplerType"), SamplerType);

    // SECURITY: Validate texturePath if provided
    if (!TexturePath.IsEmpty()) {
      FString ValidatedTexturePath = SanitizeProjectRelativePath(TexturePath);
      if (ValidatedTexturePath.IsEmpty()) {
        Bridge->SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Invalid texturePath '%s': contains traversal sequences or invalid root"), *TexturePath),
                            TEXT("INVALID_PATH"));
        return true;
      }
      TexturePath = ValidatedTexturePath;
    }

    // Resolve shared texture/sampler options first
    UTexture *ResolvedTexture = nullptr;
    if (!TexturePath.IsEmpty()) {
      ResolvedTexture = LoadObject<UTexture>(nullptr, *TexturePath);
    }
    auto ResolveSamplerType = [&SamplerType]() {
      if (SamplerType == TEXT("LinearColor")) return SAMPLERTYPE_LinearColor;
      if (SamplerType == TEXT("Normal")) return SAMPLERTYPE_Normal;
      if (SamplerType == TEXT("Masks")) return SAMPLERTYPE_Masks;
      if (SamplerType == TEXT("Alpha")) return SAMPLERTYPE_Alpha;
      return SAMPLERTYPE_Color;
    };

    UMaterialExpression *CreatedExpr = nullptr;
    if (!ParameterName.IsEmpty()) {
      UMaterialExpressionTextureSampleParameter2D *TexSample =
          NewObject<UMaterialExpressionTextureSampleParameter2D>(
              HostOuter, UMaterialExpressionTextureSampleParameter2D::StaticClass(),
              NAME_None, RF_Transactional);
      if (!TexSample) {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Failed to create texture sample expression"), TEXT("CREATION_FAILED"));
        return true;
      }
      TexSample->ParameterName = FName(*ParameterName);
      if (ResolvedTexture) TexSample->Texture = ResolvedTexture;
      TexSample->SamplerType = ResolveSamplerType();
      TexSample->MaterialExpressionEditorX = (int32)X;
      TexSample->MaterialExpressionEditorY = (int32)Y;
      CreatedExpr = TexSample;
    } else {
      UMaterialExpressionTextureSample *PlainSample =
          NewObject<UMaterialExpressionTextureSample>(
              HostOuter, UMaterialExpressionTextureSample::StaticClass(),
              NAME_None, RF_Transactional);
      if (!PlainSample) {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Failed to create texture sample expression"), TEXT("CREATION_FAILED"));
        return true;
      }
      if (ResolvedTexture) PlainSample->Texture = ResolvedTexture;
      PlainSample->SamplerType = ResolveSamplerType();
      PlainSample->MaterialExpressionEditorX = (int32)X;
      PlainSample->MaterialExpressionEditorY = (int32)Y;
      CreatedExpr = PlainSample;
    }

    AddExpressionToContainer(Material, Function, CreatedExpr);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), MCP_NODE_ID(CreatedExpr));
    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("Texture sample added."), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // add_texture_coordinate
  // --------------------------------------------------------------------------
  return false;
}
}
#endif

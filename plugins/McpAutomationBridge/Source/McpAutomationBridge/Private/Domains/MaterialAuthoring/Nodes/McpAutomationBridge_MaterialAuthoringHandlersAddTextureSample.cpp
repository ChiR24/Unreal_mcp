#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleAddTextureSample(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_texture_sample")) {
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    FString TexturePath, ParameterName;
    Payload->TryGetStringField(TEXT("texturePath"), TexturePath);
    Payload->TryGetStringField(TEXT("parameterName"), ParameterName);

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
    // A named sample is a TextureSampleParameter2D, which derives from TextureSample.
    UMaterialExpressionTextureSample *CreatedExpr = NewObject<UMaterialExpressionTextureSample>(
        HostOuter,
        ParameterName.IsEmpty() ? UMaterialExpressionTextureSample::StaticClass()
                                : UMaterialExpressionTextureSampleParameter2D::StaticClass(),
        NAME_None, RF_Transactional);
    if (UMaterialExpressionTextureSampleParameter2D *Param = Cast<UMaterialExpressionTextureSampleParameter2D>(CreatedExpr)) {
      Param->ParameterName = FName(*ParameterName);
    }
    if (ResolvedTexture) CreatedExpr->Texture = ResolvedTexture;
    CreatedExpr->MaterialExpressionEditorX = (int32)X;
    CreatedExpr->MaterialExpressionEditorY = (int32)Y;

    AddExpressionToContainer(Material, Function, CreatedExpr);
    FINALIZE_HOST();

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("nodeId"), MCP_NODE_ID(CreatedExpr));
    // Placement telemetry used to come only from the parameter-adding variants,
    // so the documented overlappingNodes / placementWarning detection could never
    // fire for the node kinds a caller stacks in a loop.
    AddMaterialNodePlacementFields(Result, Material, CreatedExpr);
    Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("Texture sample added."), Result);
    return true;
  }

  return false;
}
}

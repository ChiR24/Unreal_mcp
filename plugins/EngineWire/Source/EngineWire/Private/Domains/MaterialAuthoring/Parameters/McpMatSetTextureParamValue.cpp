#include "Domains/MaterialAuthoring/Parameters/McpMatParamValue.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleSetTextureParameterValue(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction != TEXT("set_texture_parameter_value")) {
    return false;
  }
  FString TexturePath;
  if (!Payload->TryGetStringField(TEXT("texturePath"), TexturePath) || TexturePath.IsEmpty()) {
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'texturePath'."), TEXT("INVALID_ARGUMENT"));
    return true;
  }
  // SECURITY: validate texturePath before loading it.
  const FString ValidatedTexturePath = SanitizeProjectRelativePath(TexturePath);
  if (ValidatedTexturePath.IsEmpty()) {
    Bridge->SendAutomationError(Socket, RequestId,
        McpPathRefusalMessage(TEXT("texturePath"), TexturePath),
        TEXT("INVALID_PATH"));
    return true;
  }
  UTexture* Texture = LoadObject<UTexture>(nullptr, *ValidatedTexturePath);
  if (!Texture) {
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not load texture."), TEXT("ASSET_NOT_FOUND"));
    return true;
  }
  FParameterValueWriter Writer;
  Writer.Kind = TEXT("Texture");
  Writer.Type = EMaterialParameterType::Texture;
  Writer.SetDefault = [Texture](UMaterial* Material, FName Name, TArray<FString>& Available) {
    return SetParameterExpressions<UMaterialExpressionTextureSampleParameter>(Material, Name, Available,
        [Texture](UMaterialExpressionTextureSampleParameter& Param) {
          // As the engine's own edit of the property ends: a normal map where a colour texture was failed the compile
          // on the sampler type the old texture had set.
          Param.Texture = Texture;
          Param.AutoSetSampleType();
        });
  };
  Writer.SetOverride = [Texture](UMaterialInstanceConstant* Instance, FName Name) {
    Instance->SetTextureParameterValueEditorOnly(Name, Texture);
  };
  Writer.DescribeValue = [Texture](UMaterialInterface*, FName, const TSharedPtr<FJsonObject>& Result) {
    Result->SetStringField(TEXT("texturePath"), Texture->GetPathName());
  };
  SetMaterialParameterValue(Bridge, RequestId, Payload, Socket, Writer);
  return true;
}
}

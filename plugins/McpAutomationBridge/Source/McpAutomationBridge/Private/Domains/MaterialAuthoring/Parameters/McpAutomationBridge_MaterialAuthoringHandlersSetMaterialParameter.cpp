#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleSetMaterialParameter(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("set_material_parameter")) {
    FString AssetPath, ParameterName, ParameterType;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParameterName) || ParameterName.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetStringField(TEXT("parameterType"), ParameterType);

    // SECURITY: Validate assetPath before use (accepts both Materials and Material Functions)
    FString ValidatedAssetPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedAssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid assetPath '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    Payload->SetStringField(TEXT("assetPath"), ValidatedAssetPath);

    // Each type-specific setter below already handles BOTH a material instance
    // and a base material (it edits the named parameter expression's
    // DefaultValue), so the canonical action delegates rather than refusing.
    // parameterType defaults to scalar, matching the TypeScript normalizer.
    const FString Type = ParameterType.IsEmpty() ? TEXT("scalar") : ParameterType.ToLower();

    if (Type == TEXT("scalar") || Type == TEXT("float")) {
      return HandleSetScalarParameterValue(
          Bridge, RequestId, TEXT("set_scalar_parameter_value"), Payload, Socket);
    }
    if (Type == TEXT("vector") || Type == TEXT("color")) {
      return HandleSetVectorParameterValue(
          Bridge, RequestId, TEXT("set_vector_parameter_value"), Payload, Socket);
    }
    if (Type == TEXT("texture")) {
      // The texture setter reads texturePath; a caller using the canonical
      // `value` field passes the texture asset path there instead.
      FString TexturePath, ValueString;
      if ((!Payload->TryGetStringField(TEXT("texturePath"), TexturePath) || TexturePath.IsEmpty()) &&
          Payload->TryGetStringField(TEXT("value"), ValueString) && !ValueString.IsEmpty()) {
        Payload->SetStringField(TEXT("texturePath"), ValueString);
      }
      return HandleSetTextureParameterValue(
          Bridge, RequestId, TEXT("set_texture_parameter_value"), Payload, Socket);
    }

    Bridge->SendAutomationError(Socket, RequestId,
                        FString::Printf(TEXT("Unsupported parameterType '%s'. Use scalar, vector, or texture."), *Type),
                        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  return false;
}
}
#endif

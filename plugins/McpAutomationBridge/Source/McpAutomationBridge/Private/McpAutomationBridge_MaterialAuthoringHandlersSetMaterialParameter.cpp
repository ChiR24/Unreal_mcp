#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

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
    AssetPath = ValidatedAssetPath;

    Bridge->SendAutomationError(Socket, RequestId,
                        TEXT("set_material_parameter is ambiguous. Use set_scalar_parameter_value, set_vector_parameter_value, or set_texture_parameter_value with a material instance path."),
                        TEXT("AMBIGUOUS_ACTION"));
    return true;
  }

  // --------------------------------------------------------------------------
  // get_material_node_details
  // --------------------------------------------------------------------------
  return false;
}
}
#endif

#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleSetScalarParameterValue(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("set_scalar_parameter_value")) {
    FString AssetPath, ParamName;
    double Value = 0.0;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) ||
        ParamName.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    Payload->TryGetNumberField(TEXT("value"), Value);

    // SECURITY: Validate path BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterialInstanceConstant *Instance =
        LoadObject<UMaterialInstanceConstant>(nullptr, *AssetPath);
    if (!Instance) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load material instance."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    Instance->SetScalarParameterValueEditorOnly(FName(*ParamName), Value);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialInstanceAsset(Instance);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Instance);
    Result->SetStringField(TEXT("parameterName"), ParamName);
    Result->SetNumberField(TEXT("value"), Value);
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Scalar parameter '%s' set to %f."), *ParamName,
                        Value), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // set_vector_parameter_value
  // --------------------------------------------------------------------------
  return false;
}
}
#endif

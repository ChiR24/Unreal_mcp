#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleSetVectorParameterValue(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("set_vector_parameter_value")) {
    FString AssetPath, ParamName;
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

    FLinearColor Color(1.0f, 1.0f, 1.0f, 1.0f);
    const TSharedPtr<FJsonObject> *ValueObj;
    if (Payload->TryGetObjectField(TEXT("value"), ValueObj)) {
      double R = 1.0, G = 1.0, B = 1.0, A = 1.0;
      (*ValueObj)->TryGetNumberField(TEXT("r"), R);
      (*ValueObj)->TryGetNumberField(TEXT("g"), G);
      (*ValueObj)->TryGetNumberField(TEXT("b"), B);
      (*ValueObj)->TryGetNumberField(TEXT("a"), A);
      Color = FLinearColor(R, G, B, A);
    }

    Instance->SetVectorParameterValueEditorOnly(FName(*ParamName), Color);
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
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Vector parameter '%s' set."), *ParamName), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // set_texture_parameter_value
  // --------------------------------------------------------------------------
  return false;
}
}
#endif

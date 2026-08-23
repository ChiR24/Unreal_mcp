#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"
#include "Safety/McpSafeOperationsOpenEditorGuard.h"

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
      // Fallback: a BASE UMaterial. Setting a "parameter value" on a base material means updating
      // the named ScalarParameter expression's DefaultValue — do that instead of failing with a
      // misleading ASSET_NOT_FOUND (the asset exists; it just isn't an instance).
      if (UMaterial *BaseMaterial = LoadObject<UMaterial>(nullptr, *AssetPath)) {
        // The material editor edits a preview duplicate and writes it back over
        // the original on close, so a write taken now is silently reverted the
        // moment the tab closes. Refuse instead of reporting a success the
        // caller only discovers was a lie much later.
        if (McpSafeOperations::IsAssetEditorOpen(BaseMaterial)) {
          Bridge->SendAutomationError(
              Socket, RequestId,
              McpSafeOperations::OpenAssetEditorRefusal(BaseMaterial),
              TEXT("ASSET_EDITOR_OPEN"));
          return true;
        }
        UMaterialExpressionScalarParameter *Param = nullptr;
        TArray<FString> AvailableParams;
        const FName ParamFName(*ParamName);
        // Note: with duplicate-named ScalarParameter expressions (a graph authoring error), the last
        // match wins — same ambiguity UE itself has for duplicate parameter names.
        for (UMaterialExpression *Expr : MCP_GET_MATERIAL_EXPRESSIONS(BaseMaterial)) {
          if (UMaterialExpressionScalarParameter *Scalar =
                  Cast<UMaterialExpressionScalarParameter>(Expr)) {
            AvailableParams.Add(Scalar->ParameterName.ToString());
            if (Scalar->ParameterName == ParamFName) {
              Param = Scalar;
            }
          }
        }
        if (!Param) {
          Bridge->SendAutomationError(Socket, RequestId,
                              FString::Printf(TEXT("Scalar parameter '%s' not found on base material. Available: [%s]"),
                                              *ParamName, *FString::Join(AvailableParams, TEXT(", "))),
                              TEXT("PARAMETER_NOT_FOUND"));
          return true;
        }
        Param->DefaultValue = Value;
        BaseMaterial->PostEditChange();
        BaseMaterial->MarkPackageDirty();

        bool bSaveBase = true;
        Payload->TryGetBoolField(TEXT("save"), bSaveBase);
        if (bSaveBase) {
          SaveMaterialAsset(BaseMaterial);
        }

        TSharedPtr<FJsonObject> BaseResult = McpHandlerUtils::CreateResultObject();
        McpHandlerUtils::AddVerification(BaseResult, BaseMaterial);
        BaseResult->SetStringField(TEXT("parameterName"), ParamName);
        BaseResult->SetNumberField(TEXT("value"), Value);
        BaseResult->SetStringField(TEXT("note"), TEXT("Base material (not an instance): the ScalarParameter expression's DefaultValue was updated."));
        Bridge->SendAutomationResponse(
            Socket, RequestId, true,
            FString::Printf(TEXT("Scalar parameter '%s' default set to %f on base material."),
                            *ParamName, Value), BaseResult);
        return true;
      }
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load a material instance or material at this path."),
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

  return false;
}
}
#endif

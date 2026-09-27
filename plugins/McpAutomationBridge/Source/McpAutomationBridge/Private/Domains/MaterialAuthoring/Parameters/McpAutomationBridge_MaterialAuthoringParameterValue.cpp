#include "Domains/MaterialAuthoring/Parameters/McpAutomationBridge_MaterialAuthoringParameterValue.h"
#include "Safety/McpSafeOperationsOpenEditorGuard.h"

namespace McpMaterialAuthoringHandlers
{
void SetMaterialParameterValue(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId,
                               const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket,
                               const FParameterValueWriter& Writer)
{
  FString AssetPath, ParamName;
  if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.IsEmpty()) {
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."), TEXT("INVALID_ARGUMENT"));
    return;
  }
  if (!Payload->TryGetStringField(TEXT("parameterName"), ParamName) || ParamName.IsEmpty()) {
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'parameterName'."), TEXT("INVALID_ARGUMENT"));
    return;
  }
  // SECURITY: validate the path before loading the asset.
  const FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
  if (ValidatedPath.IsEmpty()) {
    Bridge->SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
        TEXT("INVALID_PATH"));
    return;
  }
  const FName Name(*ParamName);
  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);

  UMaterialInstanceConstant* Instance =
      LoadObject<UMaterialInstanceConstant>(nullptr, *ValidatedPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
  if (!Instance) {
    // A base material keeps the value as its parameter expression's default.
    UMaterial* BaseMaterial = LoadObject<UMaterial>(nullptr, *ValidatedPath);
    if (!BaseMaterial) {
      Bridge->SendAutomationError(Socket, RequestId,
          TEXT("Could not load a material instance or material at this path."), TEXT("ASSET_NOT_FOUND"));
      return;
    }
    // The material editor edits a preview duplicate and writes it back over the
    // original on close, so a write taken now is silently reverted when the tab
    // closes. Refuse instead of reporting a success that later turns out false.
    if (McpSafeOperations::IsAssetEditorOpen(BaseMaterial)) {
      Bridge->SendAutomationError(Socket, RequestId,
          McpSafeOperations::OpenAssetEditorRefusal(BaseMaterial), TEXT("ASSET_EDITOR_OPEN"));
      return;
    }
    TArray<FString> Available;
    if (!Writer.SetDefault(BaseMaterial, Name, Available)) {
      Bridge->SendAutomationError(Socket, RequestId,
          FString::Printf(TEXT("%s parameter '%s' not found on base material. Available: [%s]"),
                          Writer.Kind, *ParamName, *FString::Join(Available, TEXT(", "))),
          TEXT("PARAMETER_NOT_FOUND"));
      return;
    }
    BaseMaterial->PostEditChange();
    BaseMaterial->MarkPackageDirty();
    if (bSave) { McpSafeAssetSave(BaseMaterial); }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, BaseMaterial);
    Result->SetStringField(TEXT("parameterName"), ParamName);
    Writer.DescribeValue(BaseMaterial, Name, Result);
    Result->SetStringField(TEXT("note"), TEXT("Base material (not an instance): the parameter expression's DefaultValue was updated."));
    Bridge->SendAutomationResponse(Socket, RequestId, true,
        FString::Printf(TEXT("%s parameter '%s' default set on base material."), Writer.Kind, *ParamName), Result);
    return;
  }

  // A name the parent does not publish used to be stored as a dead override and
  // reported as set; the caller then hunted for the bug anywhere but here.
  TArray<FMaterialParameterInfo> Infos;
  TArray<FGuid> Guids;
  Instance->GetAllParameterInfoOfType(Writer.Type, Infos, Guids);
  TArray<FString> Published;
  for (const FMaterialParameterInfo& Info : Infos) {
    Published.Add(Info.Name.ToString());
  }
  if (!Published.Contains(ParamName)) {
    Bridge->SendAutomationError(Socket, RequestId,
        FString::Printf(TEXT("%s parameter '%s' not found on this material instance. Available: [%s]"),
                        Writer.Kind, *ParamName, *FString::Join(Published, TEXT(", "))),
        TEXT("PARAMETER_NOT_FOUND"));
    return;
  }
  Writer.SetOverride(Instance, Name);
  Instance->PostEditChange();
  Instance->MarkPackageDirty();
  if (bSave) { McpSafeAssetSave(Instance); }
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  McpHandlerUtils::AddVerification(Result, Instance);
  Result->SetStringField(TEXT("parameterName"), ParamName);
  Writer.DescribeValue(Instance, Name, Result);
  Bridge->SendAutomationResponse(Socket, RequestId, true,
      FString::Printf(TEXT("%s parameter '%s' set."), Writer.Kind, *ParamName), Result);
}
}

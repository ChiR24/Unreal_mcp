#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
#include "MaterialShared.h"
#include "RHI.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleCompileMaterial(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("compile_material")) {
    // compile_material is published with `assetPath`; its alias
    // rebuild_material is published with `materialPath`. Both are dispatched
    // here, so accept either spelling or the alias is uncallable.
    FString AssetPath;
    if ((!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
         AssetPath.IsEmpty()) &&
        (!Payload->TryGetStringField(TEXT("materialPath"), AssetPath) ||
         AssetPath.IsEmpty())) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath' (or 'materialPath')."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Validate path security BEFORE loading asset
    FString ValidatedPath = SanitizeProjectRelativePath(AssetPath);
    if (ValidatedPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid root"), *AssetPath),
                          TEXT("INVALID_PATH"));
      return true;
    }
    AssetPath = ValidatedPath;

    UMaterial *Material = nullptr;
    UMaterialFunction *Function = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, Function);
    if (!Material && !Function) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("Could not load Material or Material Function."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }

    // Force recompile / update
    UObject *Host = Material ? static_cast<UObject*>(Material) : static_cast<UObject*>(Function);
    Host->PreEditChange(nullptr);
    Host->PostEditChange();
    Host->MarkPackageDirty();
    // Translation runs inside PostEditChange, so its errors are known now. A
    // material that fails to translate renders as the default material, and
    // this used to answer "compiled" regardless.
    TArray<FString> CompileErrors;
    if (Material) {
      if (const FMaterialResource *Resource = MCP_GET_MATERIAL_RESOURCE(Material)) {
        CompileErrors = Resource->GetCompileErrors();
      }
    }

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      if (Material) {
        SaveMaterialAsset(Material);
      } else {
        SaveMaterialFunctionAsset(Function);
      }
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("assetType"),
                           Material ? TEXT("Material") : TEXT("MaterialFunction"));
    Result->SetBoolField(TEXT("compiled"), CompileErrors.Num() == 0);
    TArray<TSharedPtr<FJsonValue>> ErrorValues;
    for (const FString &Error : CompileErrors) {
      ErrorValues.Add(MakeShared<FJsonValueString>(Error));
    }
    Result->SetArrayField(TEXT("compileErrors"), ErrorValues);
    Result->SetBoolField(TEXT("saved"), bSave);
    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           !Material ? FString(TEXT("Material function updated."))
                           : CompileErrors.Num() == 0 ? FString(TEXT("Material compiled."))
                           : FString::Printf(TEXT("WARNING: the material does not compile (the default material renders "
                                                  "in its place): %s"), *CompileErrors[0]),
                           Result);
    return true;
  }

  return false;
}
}
#endif

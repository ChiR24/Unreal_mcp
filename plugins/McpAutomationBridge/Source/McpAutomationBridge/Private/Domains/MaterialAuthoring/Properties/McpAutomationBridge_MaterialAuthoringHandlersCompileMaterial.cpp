#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#include "MaterialShared.h"
#include "RHI.h"

namespace McpMaterialAuthoringHandlers
{
// Translation errors are known once PostEditChange returns, but that recompile only compiles shaders as rendering
// asks for them (precompile mode None), so a Custom node's HLSL error surfaced at the next draw, after a reply
// that said "compiled". Compile every shader now, as loading the material does, and wait for them.
TArray<FString> McpMaterialCompileErrors(UMaterial* Material)
{
  if (!Material) {
    return {};
  }
  Material->ForceRecompileForRendering();
  FMaterialResource *Resource = MCP_GET_MATERIAL_RESOURCE(Material);
  if (!Resource) {
    return {};
  }
  Resource->FinishCompilation();
  TArray<FString> Errors = Resource->GetCompileErrors();
  // A shader map that failed leaves none behind, whether or not this resource kept its errors.
  if (Errors.Num() == 0 && !Resource->GetGameThreadShaderMap()) {
    Errors.Add(TEXT("No shader map compiled (LogShaderCompilers in the editor log names the failing shader); the default "
                    "material renders in its place."));
  }
  return Errors;
}

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
                          McpPathRefusalMessage(TEXT("path"), AssetPath),
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
    // A material that fails to compile renders as the default material, and this used to answer "compiled"
    // regardless.
    const TArray<FString> CompileErrors = McpMaterialCompileErrors(Material);

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      if (Material) {
        McpSafeAssetSave(Material);
      } else {
        McpSafeAssetSave(Function);
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

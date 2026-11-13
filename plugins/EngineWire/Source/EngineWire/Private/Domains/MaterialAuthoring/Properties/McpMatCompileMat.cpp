#include "Domains/MaterialAuthoring/McpMatPrivate.h"

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

bool HandleCompileMaterial(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("compile_material")) {
    // assetPath, or materialPath as its alias rebuild_material is published with.
    LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();

    // Force recompile / update
    HostOuter->PreEditChange(nullptr);
    McpRecompileMaterialHost(HostOuter);
    HostOuter->MarkPackageDirty();
    // A material that fails to compile renders as the default material, and this used to answer "compiled"
    // regardless. A function compiles nothing of its own: it answers neither field, where it said compiled.
    const TArray<FString> CompileErrors = McpMaterialCompileErrors(Material);

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    // saved echoed the request; it reports whether the save went through.
    const bool bSaved = bSave && (Material ? McpSafeAssetSave(Material) : McpSafeAssetSave(Function));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("assetPath"), AssetPath);
    Result->SetStringField(TEXT("assetType"),
                           Material ? TEXT("Material") : TEXT("MaterialFunction"));
    if (Material) {
      Result->SetBoolField(TEXT("compiled"), CompileErrors.Num() == 0);
      Result->SetArrayField(TEXT("compileErrors"), McpHandlerUtils::ToJsonStringArray(CompileErrors));
    }
    Result->SetBoolField(TEXT("saved"), bSaved);
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

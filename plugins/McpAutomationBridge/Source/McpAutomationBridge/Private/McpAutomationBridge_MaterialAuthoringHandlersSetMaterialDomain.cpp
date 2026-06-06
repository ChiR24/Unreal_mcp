#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleSetMaterialDomain(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("set_material_domain")) {
    FString AssetPath, Domain;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("materialDomain"), Domain)) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'materialDomain'."),
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
    UMaterialFunction *TmpFunc3 = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, TmpFunc3);
    if (!Material && !TmpFunc3) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    if (!Material) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("set_material_domain is only supported on UMaterial assets, not Material Functions."),
                          TEXT("UNSUPPORTED_ASSET_TYPE"));
      return true;
    }

    bool bValidDomain = false;
    if (Domain == TEXT("Surface")) {
      Material->MaterialDomain = EMaterialDomain::MD_Surface;
      bValidDomain = true;
    } else if (Domain == TEXT("DeferredDecal")) {
      Material->MaterialDomain = EMaterialDomain::MD_DeferredDecal;
      bValidDomain = true;
    } else if (Domain == TEXT("LightFunction")) {
      Material->MaterialDomain = EMaterialDomain::MD_LightFunction;
      bValidDomain = true;
    } else if (Domain == TEXT("Volume")) {
      Material->MaterialDomain = EMaterialDomain::MD_Volume;
      bValidDomain = true;
    } else if (Domain == TEXT("PostProcess")) {
      Material->MaterialDomain = EMaterialDomain::MD_PostProcess;
      bValidDomain = true;
    } else if (Domain == TEXT("UI")) {
      Material->MaterialDomain = EMaterialDomain::MD_UI;
      bValidDomain = true;
    }

    if (!bValidDomain) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid materialDomain '%s'. Valid values: Surface, DeferredDecal, LightFunction, Volume, PostProcess, UI"),
                                          *Domain),
                          TEXT("INVALID_ENUM"));
      return true;
    }

    Material->PostEditChange();
    Material->MarkPackageDirty();

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      SaveMaterialAsset(Material);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, Material);
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("Material domain set to %s."), *Domain), Result);
    return true;
  }

  return false;
}
}
#endif

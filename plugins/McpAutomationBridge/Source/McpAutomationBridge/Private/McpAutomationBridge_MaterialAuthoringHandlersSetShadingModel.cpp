#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleSetShadingModel(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("set_shading_model")) {
    FString AssetPath, ShadingModel;
    if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) ||
        AssetPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'assetPath'."),
                          TEXT("INVALID_ARGUMENT"));
      return true;
    }
    if (!Payload->TryGetStringField(TEXT("shadingModel"), ShadingModel)) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'shadingModel'."),
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
    UMaterialFunction *TmpFunc2 = nullptr;
    LoadMaterialOrFunction(AssetPath, Material, TmpFunc2);
    if (!Material && !TmpFunc2) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not load Material."),
                          TEXT("ASSET_NOT_FOUND"));
      return true;
    }
    if (!Material) {
      Bridge->SendAutomationError(Socket, RequestId,
                          TEXT("set_shading_model is only supported on UMaterial assets, not Material Functions."),
                          TEXT("UNSUPPORTED_ASSET_TYPE"));
      return true;
    }

    bool bValidShadingModel = false;
    if (ShadingModel == TEXT("Unlit")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_Unlit);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("DefaultLit")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_DefaultLit);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("Subsurface")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_Subsurface);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("SubsurfaceProfile")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_SubsurfaceProfile);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("PreintegratedSkin")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_PreintegratedSkin);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("ClearCoat")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_ClearCoat);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("Hair")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_Hair);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("Cloth")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_Cloth);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("Eye")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_Eye);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("TwoSidedFoliage")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_TwoSidedFoliage);
      bValidShadingModel = true;
    } else if (ShadingModel == TEXT("ThinTranslucent")) {
      Material->SetShadingModel(EMaterialShadingModel::MSM_ThinTranslucent);
      bValidShadingModel = true;
    }

    if (!bValidShadingModel) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid shadingModel '%s'. Valid values: Unlit, DefaultLit, Subsurface, SubsurfaceProfile, PreintegratedSkin, ClearCoat, Hair, Cloth, Eye, TwoSidedFoliage, ThinTranslucent"),
                                          *ShadingModel),
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
        FString::Printf(TEXT("Shading model set to %s."), *ShadingModel), Result);
    return true;
  }

  // --------------------------------------------------------------------------
  // set_material_domain
  // --------------------------------------------------------------------------
  return false;
}
}
#endif

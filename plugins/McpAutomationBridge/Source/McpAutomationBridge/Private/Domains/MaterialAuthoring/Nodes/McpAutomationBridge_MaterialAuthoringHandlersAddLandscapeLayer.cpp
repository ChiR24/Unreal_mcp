#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

namespace McpMaterialAuthoringHandlers
{
bool HandleAddLandscapeLayer(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_landscape_layer")) {
    FString LayerName;
    if (!Payload->TryGetStringField(TEXT("layerName"), LayerName) || LayerName.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'layerName'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // The layer info asset goes in `path` when given, else beside the material.
    // materialPath names the material, not a folder: it used to be taken as the
    // folder, so a material path grew a fake folder named after the material.
    FString Path, MaterialRef;
    Payload->TryGetStringField(TEXT("path"), Path);
    if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialRef) || MaterialRef.IsEmpty()) {
      Payload->TryGetStringField(TEXT("assetPath"), MaterialRef);
    }
    if (Path.IsEmpty() && !MaterialRef.IsEmpty()) {
      const FString SafeMaterial = SanitizeProjectRelativePath(MaterialRef);
      if (SafeMaterial.IsEmpty() || !LoadObject<UMaterialInterface>(nullptr, *SafeMaterial)) {
        Bridge->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("materialPath '%s' is not a material; pass path to choose the layer info folder instead."), *MaterialRef),
            TEXT("ASSET_NOT_FOUND"));
        return true;
      }
      Path = FPackageName::GetLongPackagePath(FPackageName::ObjectPathToPackageName(SafeMaterial));
    }
    if (Path.IsEmpty()) { Path = TEXT("/Game/Landscape/Layers"); }

    // Validate path security - reject traversal and invalid paths
    FString ValidatedPath = SanitizeProjectRelativePath(Path);
    if (ValidatedPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid characters"), *Path),
                          TEXT("INVALID_PATH"));
      return true;
    }
    Path = ValidatedPath;

    FString PackagePath = Path / LayerName;
    if (!FPackageName::IsValidLongPackageName(PackagePath)) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid package path: %s"), *PackagePath),
                          TEXT("INVALID_PATH"));
      return true;
    }

    // Resolve the physical material before creating anything, so a bad path
    // leaves no half-made layer info behind (it used to be skipped silently).
    UPhysicalMaterial* PhysMat = nullptr;
    FString PhysMaterialPath;
    if (Payload->TryGetStringField(TEXT("physicalMaterialPath"), PhysMaterialPath) && !PhysMaterialPath.IsEmpty()) {
      // SECURITY: Validate physicalMaterialPath before loading
      FString ValidatedPhysMatPath = SanitizeProjectRelativePath(PhysMaterialPath);
      if (ValidatedPhysMatPath.IsEmpty()) {
        Bridge->SendAutomationError(Socket, RequestId,
                            FString::Printf(TEXT("Invalid physicalMaterialPath '%s': contains traversal sequences or invalid root"), *PhysMaterialPath),
                            TEXT("INVALID_PATH"));
        return true;
      }
      PhysMat = LoadObject<UPhysicalMaterial>(nullptr, *ValidatedPhysMatPath);
      if (!PhysMat) {
        Bridge->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("physicalMaterialPath '%s' is not a physical material."), *ValidatedPhysMatPath), TEXT("ASSET_NOT_FOUND"));
        return true;
      }
    }

    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Failed to create package."), TEXT("PACKAGE_ERROR"));
      return true;
    }

    ULandscapeLayerInfoObject* LayerInfo = NewObject<ULandscapeLayerInfoObject>(
        Package, FName(*LayerName), RF_Public | RF_Standalone);

    if (!LayerInfo) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Failed to create layer info."), TEXT("CREATION_ERROR"));
      return true;
    }

PRAGMA_DISABLE_DEPRECATION_WARNINGS
    LayerInfo->LayerName = FName(*LayerName);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

    double Hardness = 0.5;
    if (Payload->TryGetNumberField(TEXT("hardness"), Hardness)) {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
      LayerInfo->Hardness = static_cast<float>(Hardness);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }

    if (PhysMat) {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
      LayerInfo->PhysMaterial = PhysMat;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }

    // Set blend method if specified (replaces bNoWeightBlend)
    bool bNoWeightBlend = false;
    if (Payload->TryGetBoolField(TEXT("noWeightBlend"), bNoWeightBlend)) {
#if ENGINE_MINOR_VERSION >= 7
      // UE 5.7+: Use SetBlendMethod with ELandscapeTargetLayerBlendMethod
      LayerInfo->SetBlendMethod(bNoWeightBlend ? ELandscapeTargetLayerBlendMethod::None : ELandscapeTargetLayerBlendMethod::FinalWeightBlending, false);
#else
      // UE 5.0-5.6: Use direct bNoWeightBlend property
      LayerInfo->bNoWeightBlend = bNoWeightBlend;
#endif
    }

    FAssetRegistryModule::AssetCreated(LayerInfo);

    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    const bool bSaved = bSave && McpSafeAssetSave(LayerInfo);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, LayerInfo);
    Result->SetStringField(TEXT("layerName"), LayerName);
    Result->SetStringField(TEXT("layerInfoPath"), LayerInfo->GetPathName());
    Result->SetBoolField(TEXT("saved"), bSaved);

    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Landscape layer '%s' created."), *LayerName),
                           Result);
    return true;
  }
  return false;
}
}

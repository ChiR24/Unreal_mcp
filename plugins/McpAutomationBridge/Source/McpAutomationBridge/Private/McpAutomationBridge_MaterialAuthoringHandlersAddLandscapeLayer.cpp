#include "McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"

#if WITH_EDITOR
namespace McpMaterialAuthoringHandlers
{
bool HandleAddLandscapeLayer(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction == TEXT("add_landscape_layer")) {
#if MCP_HAS_LANDSCAPE_LAYER
    FString LayerName;
    if (!Payload->TryGetStringField(TEXT("layerName"), LayerName) || LayerName.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId, TEXT("Missing 'layerName'."), TEXT("INVALID_ARGUMENT"));
      return true;
    }

    // Accept path via multiple parameter names (assetPath, materialPath, or path)
    FString Path;
    if (Payload->TryGetStringField(TEXT("assetPath"), Path) && !Path.IsEmpty()) {
      // Use assetPath
    } else if (Payload->TryGetStringField(TEXT("materialPath"), Path) && !Path.IsEmpty()) {
      // Use materialPath
    } else if (Payload->TryGetStringField(TEXT("path"), Path) && !Path.IsEmpty()) {
      // Use path
    } else {
      Path = TEXT("/Game/Landscape/Layers");
    }

    // Validate path security - reject traversal and invalid paths
    FString ValidatedPath = SanitizeProjectRelativePath(Path);
    if (ValidatedPath.IsEmpty()) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid path '%s': contains traversal sequences or invalid characters"), *Path),
                          TEXT("INVALID_PATH"));
      return true;
    }
    Path = ValidatedPath;

    // Validate the full package path
    FString PackagePath = Path / LayerName;
    if (!FPackageName::IsValidLongPackageName(PackagePath)) {
      Bridge->SendAutomationError(Socket, RequestId,
                          FString::Printf(TEXT("Invalid package path: %s"), *PackagePath),
                          TEXT("INVALID_PATH"));
      return true;
    }

    // Create the landscape layer info asset
    FString PackageName = PackagePath;
    UPackage* Package = CreatePackage(*PackageName);
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

    // Set layer name
PRAGMA_DISABLE_DEPRECATION_WARNINGS
    LayerInfo->LayerName = FName(*LayerName);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

    // Set optional properties
    double Hardness = 0.5;
    if (Payload->TryGetNumberField(TEXT("hardness"), Hardness)) {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
      LayerInfo->Hardness = static_cast<float>(Hardness);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }

    // Set physical material if provided
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
      PhysMaterialPath = ValidatedPhysMatPath;

      UPhysicalMaterial* PhysMat = LoadObject<UPhysicalMaterial>(nullptr, *PhysMaterialPath);
      if (PhysMat) {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
        LayerInfo->PhysMaterial = PhysMat;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
      }
    }

#if WITH_EDITORONLY_DATA
    // Set blend method if specified (replaces bNoWeightBlend)
    bool bNoWeightBlend = false;
    if (Payload->TryGetBoolField(TEXT("noWeightBlend"), bNoWeightBlend)) {
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
      // UE 5.7+: Use SetBlendMethod with ELandscapeTargetLayerBlendMethod
      LayerInfo->SetBlendMethod(bNoWeightBlend ? ELandscapeTargetLayerBlendMethod::None : ELandscapeTargetLayerBlendMethod::FinalWeightBlending, false);
#else
      // UE 5.0-5.6: Use direct bNoWeightBlend property
      LayerInfo->bNoWeightBlend = bNoWeightBlend;
#endif
    }
#endif

    // Save the asset
    bool bSave = true;
    Payload->TryGetBoolField(TEXT("save"), bSave);
    if (bSave) {
      FString AssetPathStr = LayerInfo->GetPathName();
      int32 DotIndex = AssetPathStr.Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
      if (DotIndex != INDEX_NONE) { AssetPathStr.LeftInline(DotIndex); }
      LayerInfo->MarkPackageDirty();
    }

    // Notify asset registry
    FAssetRegistryModule::AssetCreated(LayerInfo);

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(Result, LayerInfo);
    Result->SetStringField(TEXT("layerName"), LayerName);

    Bridge->SendAutomationResponse(Socket, RequestId, true,
                           FString::Printf(TEXT("Landscape layer '%s' created."), *LayerName),
                           Result);
    return true;
#else
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Landscape module not available."), TEXT("NOT_SUPPORTED"));
    return true;
#endif
  }
  return false;
}
}
#endif

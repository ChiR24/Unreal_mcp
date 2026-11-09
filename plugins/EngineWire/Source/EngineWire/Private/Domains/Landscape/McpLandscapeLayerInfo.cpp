#include "Domains/Environment/Runtime/McpEnvAssetValidate.h"
#include "Domains/Environment/McpEnvShared.h"
#include "Domains/Landscape/McpLandscapeShared.h"

namespace McpLandscapeHandlers {

void SetLayerInfoName(ULandscapeLayerInfoObject *LayerInfo, FName LayerName)
{
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
    LayerInfo->SetLayerName(LayerName, true);
#else
PRAGMA_DISABLE_DEPRECATION_WARNINGS
    LayerInfo->LayerName = LayerName;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
}

bool McpCreateLandscapeLayerInfo(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                        FString &OutMessage, FString &OutErrorCode)
{
    FString LayerName = McpGetFirstStringField(Payload, {TEXT("layerName"), TEXT("name")});
    if (LayerName.IsEmpty())
    {
        return McpEnvironmentHandlers::McpFailEnvironmentAction(OutMessage, OutErrorCode,
            TEXT("layerName or name required for create_landscape_layer_info"), TEXT("INVALID_ARGUMENT"));
    }

    FString Path = McpGetFirstStringField(Payload, {TEXT("path"), TEXT("layerInfoPath")});
    if (Path.IsEmpty())
    {
        Path = TEXT("/Game/Landscape");
    }
    FString PackagePath;
    if (!McpEnvironmentHandlers::McpBuildValidatedEnvironmentAssetPath(Path, LayerName, TEXT("layer info"),
                                               PackagePath, OutMessage, OutErrorCode))
    {
        return false;
    }
    // A physical material that does not load fails the call; it used to be dropped behind a success.
    const FString PhysicalMaterialPath = GetJsonStringField(Payload, TEXT("physicalMaterialPath"));
    UPhysicalMaterial *PhysicalMaterial = PhysicalMaterialPath.IsEmpty() ? nullptr : LoadObject<UPhysicalMaterial>(nullptr, *PhysicalMaterialPath);
    if (!PhysicalMaterialPath.IsEmpty() && !PhysicalMaterial)
    {
        return McpEnvironmentHandlers::McpFailEnvironmentAction(OutMessage, OutErrorCode,
            FString::Printf(TEXT("Physical material not found: %s"), *PhysicalMaterialPath), TEXT("ASSET_NOT_FOUND"));
    }

    FString CreateError, CreateCode;
    UPackage* Package = McpCreatePackageForNewAsset(PackagePath, CreateError, CreateCode);
    if (!Package)
    {
        return McpEnvironmentHandlers::McpFailEnvironmentAction(OutMessage, OutErrorCode, CreateError, *CreateCode);
    }
    ULandscapeLayerInfoObject *LayerInfo = NewObject<ULandscapeLayerInfoObject>(Package, FName(*LayerName), RF_Public | RF_Standalone);
    if (!LayerInfo)
    {
        return McpEnvironmentHandlers::McpFailEnvironmentAction(OutMessage, OutErrorCode,
            TEXT("Failed to create landscape layer info"), TEXT("CREATION_FAILED"));
    }

    SetLayerInfoName(LayerInfo, FName(*LayerName));

    double Hardness = 0.0;
    if (Payload->TryGetNumberField(TEXT("hardness"), Hardness))
    {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
        LayerInfo->Hardness = static_cast<float>(Hardness);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }

    if (PhysicalMaterial)
    {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
        LayerInfo->PhysMaterial = PhysicalMaterial;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }

    bool bNoWeightBlend = false;
    if (Payload->TryGetBoolField(TEXT("noWeightBlend"), bNoWeightBlend))
    {
#if ENGINE_MINOR_VERSION >= 7
        LayerInfo->SetBlendMethod(bNoWeightBlend ? ELandscapeTargetLayerBlendMethod::None : ELandscapeTargetLayerBlendMethod::FinalWeightBlending, false);
#else
        LayerInfo->bNoWeightBlend = bNoWeightBlend;
#endif
    }

    FAssetRegistryModule::AssetCreated(LayerInfo);
    LayerInfo->MarkPackageDirty();
    if (!McpSafeAssetSave(LayerInfo))
    {
        return McpEnvironmentHandlers::McpFailEnvironmentAction(OutMessage, OutErrorCode,
            TEXT("Failed to save landscape layer info"), TEXT("SAVE_FAILED"));
    }

    Resp->SetStringField(TEXT("layerName"), LayerName);
    Resp->SetStringField(TEXT("assetPath"), LayerInfo->GetPathName());
    McpHandlerUtils::AddVerification(Resp, LayerInfo);
    OutMessage = TEXT("Landscape layer info created");
    return true;
}

} // namespace McpLandscapeHandlers

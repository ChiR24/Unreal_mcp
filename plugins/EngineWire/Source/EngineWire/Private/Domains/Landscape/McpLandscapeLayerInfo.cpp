#include "Foundation/BridgeHelpers/Assets/McpAssetCreation.h"
#include "Foundation/BridgeHelpers/Responses/McpResponses.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "LandscapeLayerInfoObject.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Safety/McpSafeOperations.h"
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

ULandscapeLayerInfoObject *McpNewLandscapeLayerInfo(const FString &PackagePath, const FString &LayerName,
                                                    const TSharedPtr<FJsonObject> &Payload, UPhysicalMaterial *PhysicalMaterial,
                                                    FString &OutError, FString &OutCode)
{
    UPackage* Package = McpCreatePackageForNewAsset(PackagePath, OutError, OutCode);
    if (!Package)
    {
        return nullptr;
    }
    ULandscapeLayerInfoObject *LayerInfo = NewObject<ULandscapeLayerInfoObject>(Package, FName(*LayerName), RF_Public | RF_Standalone | RF_Transactional);
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
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
        LayerInfo->SetBlendMethod(bNoWeightBlend ? ELandscapeTargetLayerBlendMethod::None : ELandscapeTargetLayerBlendMethod::FinalWeightBlending, false);
#else
        LayerInfo->bNoWeightBlend = bNoWeightBlend;
#endif
    }

    FAssetRegistryModule::AssetCreated(LayerInfo);
    return LayerInfo;
}

bool McpCreateLandscapeLayerInfo(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                        FString &OutMessage, FString &OutErrorCode)
{
    FString LayerName = McpGetFirstStringField(Payload, {TEXT("layerName"), TEXT("name")});
    if (LayerName.IsEmpty())
    {
        return McpRefuse(OutMessage, OutErrorCode,
            TEXT("layerName or name required for create_landscape_layer_info"), TEXT("INVALID_ARGUMENT"));
    }

    FString Path = GetJsonStringField(Payload, TEXT("path"));
    if (Path.IsEmpty())
    {
        Path = TEXT("/Game/Landscape");
    }
    FString PackagePath;
    if (!McpBuildValidatedAssetPath(Path, LayerName, TEXT("layer info"),
                                    PackagePath, OutMessage, OutErrorCode))
    {
        return false;
    }
    // A physical material that does not load fails the call; it used to be dropped behind a success.
    const FString PhysicalMaterialPath = GetJsonStringField(Payload, TEXT("physicalMaterialPath"));
    UPhysicalMaterial *PhysicalMaterial = PhysicalMaterialPath.IsEmpty() ? nullptr : LoadObject<UPhysicalMaterial>(nullptr, *PhysicalMaterialPath);
    if (!PhysicalMaterialPath.IsEmpty() && !PhysicalMaterial)
    {
        return McpRefuse(OutMessage, OutErrorCode,
            FString::Printf(TEXT("Physical material not found: %s"), *PhysicalMaterialPath), TEXT("ASSET_NOT_FOUND"));
    }

    FString CreateError, CreateCode;
    ULandscapeLayerInfoObject *LayerInfo =
        McpNewLandscapeLayerInfo(PackagePath, LayerName, Payload, PhysicalMaterial, CreateError, CreateCode);
    if (!LayerInfo)
    {
        return McpRefuse(OutMessage, OutErrorCode, CreateError, *CreateCode);
    }
    LayerInfo->MarkPackageDirty();
    if (!McpSafeAssetSave(LayerInfo))
    {
        return McpRefuse(OutMessage, OutErrorCode,
            TEXT("Failed to save landscape layer info"), TEXT("SAVE_FAILED"));
    }

    Resp->SetStringField(TEXT("layerName"), LayerName);
    Resp->SetStringField(TEXT("assetPath"), LayerInfo->GetPathName());
    McpHandlerUtils::AddVerification(Resp, LayerInfo);
    OutMessage = TEXT("Landscape layer info created");
    return true;
}

} // namespace McpLandscapeHandlers

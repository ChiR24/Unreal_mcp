#include "McpAutomationBridge_EnvironmentHandlersShared.h"

#if WITH_EDITOR
namespace McpEnvironmentHandlers {

UFoliageType *McpLoadFoliageTypeFromPayload(const TSharedPtr<FJsonObject> &Payload, FString &OutPath)
{
    OutPath = McpGetFirstStringField(Payload, {TEXT("foliageTypePath"), TEXT("foliageType"), TEXT("path")});
    if (OutPath.IsEmpty())
    {
        return nullptr;
    }
    FString SafePath = SanitizeProjectRelativePath(OutPath);
    if (SafePath.IsEmpty())
    {
        return nullptr;
    }
    OutPath = SafePath;
    return LoadObject<UFoliageType>(nullptr, *OutPath);
}
bool McpConfigureFoliageType(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                    FString &OutMessage, FString &OutErrorCode)
{
    FString FoliagePath;
    UFoliageType *FoliageType = McpLoadFoliageTypeFromPayload(Payload, FoliagePath);
    if (!FoliageType)
    {
        OutMessage = TEXT("Foliage type asset not found");
        OutErrorCode = TEXT("ASSET_NOT_FOUND");
        return false;
    }

    if (UFoliageType_InstancedStaticMesh *Instanced = Cast<UFoliageType_InstancedStaticMesh>(FoliageType))
    {
        FString MeshPath = McpGetFirstStringField(Payload, {TEXT("meshPath"), TEXT("staticMesh")});
        if (!MeshPath.IsEmpty())
        {
            if (UStaticMesh *Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath))
            {
                Instanced->SetStaticMesh(Mesh);
                Resp->SetStringField(TEXT("meshPath"), MeshPath);
            }
        }
    }

    double NumberValue = 0.0;
    if (Payload->TryGetNumberField(TEXT("density"), NumberValue))
    {
        FoliageType->Density = static_cast<float>(NumberValue);
    }

    double MinScale = 0.0;
    double MaxScale = 0.0;
    if (Payload->TryGetNumberField(TEXT("minScale"), MinScale) || Payload->TryGetNumberField(TEXT("maxScale"), MaxScale))
    {
        if (MinScale <= 0.0)
        {
            MinScale = FoliageType->ScaleX.Min;
        }
        if (MaxScale <= 0.0)
        {
            MaxScale = FoliageType->ScaleX.Max;
        }
        FoliageType->Scaling = EFoliageScaling::Uniform;
        FoliageType->ScaleX = FFloatInterval(static_cast<float>(MinScale), static_cast<float>(MaxScale));
        FoliageType->ScaleY = FoliageType->ScaleX;
        FoliageType->ScaleZ = FoliageType->ScaleX;
    }

    bool BoolValue = false;
    if (Payload->TryGetBoolField(TEXT("alignToNormal"), BoolValue))
    {
        FoliageType->AlignToNormal = BoolValue;
    }
    if (Payload->TryGetBoolField(TEXT("randomYaw"), BoolValue))
    {
        FoliageType->RandomYaw = BoolValue;
    }

    int32 CullDistance = 0;
    if (Payload->TryGetNumberField(TEXT("cullDistance"), CullDistance))
    {
        FoliageType->CullDistance.Max = CullDistance;
    }

    TArray<FString> Applied;
    TArray<FString> Failed;
    const int32 ReflectedCount = McpApplyPayloadSettings(FoliageType, Payload, Applied, Failed);
    FoliageType->MarkPackageDirty();
    McpSafeAssetSave(FoliageType);

    Resp->SetStringField(TEXT("foliageTypePath"), FoliagePath);
    Resp->SetNumberField(TEXT("configuredPropertyCount"), ReflectedCount);
    McpAddStringArrayField(Resp, TEXT("configuredProperties"), Applied);
    McpAddStringArrayField(Resp, TEXT("configurationErrors"), Failed);
    McpHandlerUtils::AddVerification(Resp, FoliageType);
    OutMessage = TEXT("Foliage type configured");
    return true;
}
bool McpCreateLandscapeLayerInfo(const TSharedPtr<FJsonObject> &Payload, TSharedPtr<FJsonObject> Resp,
                                        FString &OutMessage, FString &OutErrorCode)
{
    FString LayerName = McpGetFirstStringField(Payload, {TEXT("layerName"), TEXT("name")});
    if (LayerName.IsEmpty())
    {
        OutMessage = TEXT("layerName or name required for create_landscape_layer_info");
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }

    FString Path = McpGetFirstStringField(Payload, {TEXT("path"), TEXT("layerInfoPath")});
    if (Path.IsEmpty())
    {
        Path = TEXT("/Game/Landscape");
    }
    FString SafePath = SanitizeProjectRelativePath(Path);
    if (SafePath.IsEmpty())
    {
        OutMessage = FString::Printf(TEXT("Invalid layer info path: %s"), *Path);
        OutErrorCode = TEXT("SECURITY_VIOLATION");
        return false;
    }

    FString PackagePath = SafePath;
    if (!FPaths::GetBaseFilename(PackagePath).Equals(LayerName, ESearchCase::IgnoreCase))
    {
        PackagePath = SafePath / LayerName;
    }

    UPackage *Package = CreatePackage(*PackagePath);
    ULandscapeLayerInfoObject *LayerInfo = NewObject<ULandscapeLayerInfoObject>(Package, FName(*LayerName), RF_Public | RF_Standalone);
    if (!LayerInfo)
    {
        OutMessage = TEXT("Failed to create landscape layer info");
        OutErrorCode = TEXT("CREATION_FAILED");
        return false;
    }

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
    LayerInfo->SetLayerName(FName(*LayerName), true);
#else
PRAGMA_DISABLE_DEPRECATION_WARNINGS
    LayerInfo->LayerName = FName(*LayerName);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif

    double Hardness = 0.0;
    if (Payload->TryGetNumberField(TEXT("hardness"), Hardness))
    {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
        LayerInfo->Hardness = static_cast<float>(Hardness);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }

    FString PhysicalMaterialPath;
    if (Payload->TryGetStringField(TEXT("physicalMaterialPath"), PhysicalMaterialPath) && !PhysicalMaterialPath.IsEmpty())
    {
        if (UPhysicalMaterial *PhysicalMaterial = LoadObject<UPhysicalMaterial>(nullptr, *PhysicalMaterialPath))
        {
PRAGMA_DISABLE_DEPRECATION_WARNINGS
            LayerInfo->PhysMaterial = PhysicalMaterial;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
        }
    }

    bool bNoWeightBlend = false;
    if (Payload->TryGetBoolField(TEXT("noWeightBlend"), bNoWeightBlend))
    {
#if WITH_EDITORONLY_DATA
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 7
        LayerInfo->SetBlendMethod(bNoWeightBlend ? ELandscapeTargetLayerBlendMethod::None : ELandscapeTargetLayerBlendMethod::FinalWeightBlending, false);
#else
        LayerInfo->bNoWeightBlend = bNoWeightBlend;
#endif
#endif
    }

    FAssetRegistryModule::AssetCreated(LayerInfo);
    LayerInfo->MarkPackageDirty();
    McpSafeAssetSave(LayerInfo);

    Resp->SetStringField(TEXT("layerName"), LayerName);
    Resp->SetStringField(TEXT("assetPath"), LayerInfo->GetPathName());
    McpHandlerUtils::AddVerification(Resp, LayerInfo);
    OutMessage = TEXT("Landscape layer info created");
    return true;
}
bool McpCreateLinearColorCurve(const TSharedPtr<FJsonObject> &Payload, const FString &DefaultName,
                                      TSharedPtr<FJsonObject> Resp, FString &OutMessage, FString &OutErrorCode)
{
    FString Name = McpGetFirstStringField(Payload, {TEXT("name"), TEXT("curveName")});
    if (Name.IsEmpty())
    {
        Name = DefaultName;
    }
    FString Path = McpGetFirstStringField(Payload, {TEXT("path"), TEXT("curvePath")});
    if (Path.IsEmpty())
    {
        Path = TEXT("/Game/Environment/Curves");
    }
    FString SafePath = SanitizeProjectRelativePath(Path);
    if (SafePath.IsEmpty())
    {
        OutMessage = FString::Printf(TEXT("Invalid curve path: %s"), *Path);
        OutErrorCode = TEXT("SECURITY_VIOLATION");
        return false;
    }

    FString PackagePath = SafePath;
    if (!FPaths::GetBaseFilename(PackagePath).Equals(Name, ESearchCase::IgnoreCase))
    {
        PackagePath = SafePath / Name;
    }
    UPackage *Package = CreatePackage(*PackagePath);
    UCurveLinearColor *Curve = NewObject<UCurveLinearColor>(Package, FName(*Name), RF_Public | RF_Standalone);
    if (!Curve)
    {
        OutMessage = TEXT("Failed to create color curve");
        OutErrorCode = TEXT("CREATION_FAILED");
        return false;
    }

    Curve->FloatCurves[0].UpdateOrAddKey(0.0f, 1.0f);
    Curve->FloatCurves[1].UpdateOrAddKey(0.0f, 1.0f);
    Curve->FloatCurves[2].UpdateOrAddKey(0.0f, 1.0f);
    Curve->FloatCurves[3].UpdateOrAddKey(0.0f, 1.0f);
    FAssetRegistryModule::AssetCreated(Curve);
    Curve->MarkPackageDirty();
    McpSafeAssetSave(Curve);

    Resp->SetStringField(TEXT("curvePath"), Curve->GetPathName());
    McpHandlerUtils::AddVerification(Resp, Curve);
    OutMessage = TEXT("Color curve created");
    return true;
}
UFoliageType *McpLoadFoliageTypeForEnvironmentAction(const TSharedPtr<FJsonObject> &Payload)
{
    FString FoliagePath;
    return McpLoadFoliageTypeFromPayload(Payload, FoliagePath);
}

} // namespace McpEnvironmentHandlers
#endif

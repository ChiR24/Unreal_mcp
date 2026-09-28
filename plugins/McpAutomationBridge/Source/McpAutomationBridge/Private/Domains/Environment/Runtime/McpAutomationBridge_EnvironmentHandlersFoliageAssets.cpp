#include "Domains/Environment/Runtime/McpAutomationBridge_EnvironmentAssetValidation.h"
#include "Domains/Environment/McpAutomationBridge_EnvironmentHandlersShared.h"

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
        return McpFailEnvironmentAction(OutMessage, OutErrorCode, TEXT("Foliage type asset not found"), TEXT("ASSET_NOT_FOUND"));
    }
    FoliageType->Modify();

    // A mesh that did not load used to be skipped, keeping the old mesh behind a success.
    const FString MeshPath = McpGetFirstStringField(Payload, {TEXT("meshPath"), TEXT("staticMesh")});
    if (!MeshPath.IsEmpty())
    {
        UFoliageType_InstancedStaticMesh *Instanced = Cast<UFoliageType_InstancedStaticMesh>(FoliageType);
        UStaticMesh *Mesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
        if (!Instanced || !Mesh)
        {
            return McpFailEnvironmentAction(OutMessage, OutErrorCode,
                Instanced ? FString::Printf(TEXT("Static mesh not found: %s"), *MeshPath)
                          : FString(TEXT("This foliage type does not take a static mesh")),
                Instanced ? TEXT("ASSET_NOT_FOUND") : TEXT("INVALID_ARGUMENT"));
        }
        Instanced->SetStaticMesh(Mesh);
        Resp->SetStringField(TEXT("meshPath"), MeshPath);
    }

    double NumberValue = 0.0;
    if (Payload->TryGetNumberField(TEXT("density"), NumberValue))
    {
        FoliageType->Density = static_cast<float>(NumberValue);
    }

    // Both bounds are read: a || here once skipped maxScale whenever minScale was given.
    double MinScale = 0.0;
    double MaxScale = 0.0;
    const bool bHasMin = Payload->TryGetNumberField(TEXT("minScale"), MinScale);
    const bool bHasMax = Payload->TryGetNumberField(TEXT("maxScale"), MaxScale);
    if (bHasMin || bHasMax)
    {
        MinScale = bHasMin ? MinScale : FoliageType->ScaleX.Min;
        MaxScale = bHasMax ? MaxScale : FoliageType->ScaleX.Max;
        if (MinScale <= 0.0 || MaxScale < MinScale)
        {
            return McpFailEnvironmentAction(OutMessage, OutErrorCode,
                TEXT("minScale must be positive and no greater than maxScale"), TEXT("INVALID_ARGUMENT"));
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
    // UFoliageType has no CollisionEnabled property; placed instances take BodyInstance's.
    if (Payload->TryGetBoolField(TEXT("collisionEnabled"), BoolValue))
    {
        FoliageType->BodyInstance.SetCollisionEnabled(BoolValue ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        Resp->SetBoolField(TEXT("collisionEnabled"), BoolValue);
    }

    // The cull distance; the fade start (Min) only moves when it would pass it.
    int32 CullDistance = 0;
    if (Payload->TryGetNumberField(TEXT("cullDistance"), CullDistance))
    {
        FoliageType->CullDistance.Max = FMath::Max(CullDistance, 0);
        FoliageType->CullDistance.Min = FMath::Min(FoliageType->CullDistance.Min, FoliageType->CullDistance.Max);
        Resp->SetNumberField(TEXT("cullDistance"), FoliageType->CullDistance.Max);
    }

    // The keys handled above stay out of the reflection pass, which failed
    // writing the cullDistance number into the CullDistance interval.
    TSharedPtr<FJsonObject> ReflectPayload = MakeShared<FJsonObject>(*Payload);
    for (const TCHAR *Handled : {TEXT("density"), TEXT("minScale"), TEXT("maxScale"), TEXT("alignToNormal"),
                                 TEXT("randomYaw"), TEXT("collisionEnabled"), TEXT("cullDistance")})
    {
        ReflectPayload->RemoveField(Handled);
    }
    TArray<FString> Applied;
    TArray<FString> Failed;
    const int32 ReflectedCount = McpApplyPayloadSettings(FoliageType, ReflectPayload, Applied, Failed);
    if (Failed.Num() > 0)
    {
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            FString::Printf(TEXT("Could not apply: %s"), *FString::Join(Failed, TEXT("; "))), TEXT("CONFIGURATION_FAILED"));
    }
    FoliageType->PostEditChange();
    FoliageType->MarkPackageDirty();
    if (!McpSafeAssetSave(FoliageType))
    {
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            TEXT("Failed to save foliage type"), TEXT("SAVE_FAILED"));
    }

    Resp->SetStringField(TEXT("foliageTypePath"), FoliagePath);
    Resp->SetNumberField(TEXT("configuredPropertyCount"), ReflectedCount);
    McpAddStringArrayField(Resp, TEXT("configuredProperties"), Applied);
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
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            TEXT("layerName or name required for create_landscape_layer_info"), TEXT("INVALID_ARGUMENT"));
    }

    FString Path = McpGetFirstStringField(Payload, {TEXT("path"), TEXT("layerInfoPath")});
    if (Path.IsEmpty())
    {
        Path = TEXT("/Game/Landscape");
    }
    FString PackagePath;
    if (!McpBuildValidatedEnvironmentAssetPath(Path, LayerName, TEXT("layer info"),
                                               PackagePath, OutMessage, OutErrorCode))
    {
        return false;
    }
    // A physical material that does not load fails the call; it used to be dropped behind a success.
    const FString PhysicalMaterialPath = GetJsonStringField(Payload, TEXT("physicalMaterialPath"));
    UPhysicalMaterial *PhysicalMaterial = PhysicalMaterialPath.IsEmpty() ? nullptr : LoadObject<UPhysicalMaterial>(nullptr, *PhysicalMaterialPath);
    if (!PhysicalMaterialPath.IsEmpty() && !PhysicalMaterial)
    {
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            FString::Printf(TEXT("Physical material not found: %s"), *PhysicalMaterialPath), TEXT("ASSET_NOT_FOUND"));
    }

    UPackage *Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            TEXT("Failed to create landscape layer package"), TEXT("PACKAGE_CREATION_FAILED"));
    }
    ULandscapeLayerInfoObject *LayerInfo = NewObject<ULandscapeLayerInfoObject>(Package, FName(*LayerName), RF_Public | RF_Standalone);
    if (!LayerInfo)
    {
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            TEXT("Failed to create landscape layer info"), TEXT("CREATION_FAILED"));
    }

#if ENGINE_MINOR_VERSION >= 7
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
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            TEXT("Failed to save landscape layer info"), TEXT("SAVE_FAILED"));
    }

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
    FString PackagePath;
    if (!McpBuildValidatedEnvironmentAssetPath(Path, Name, TEXT("color curve"),
                                               PackagePath, OutMessage, OutErrorCode))
    {
        return false;
    }

    UPackage *Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            TEXT("Failed to create color curve package"), TEXT("PACKAGE_CREATION_FAILED"));
    }
    UCurveLinearColor *Curve = NewObject<UCurveLinearColor>(Package, FName(*Name), RF_Public | RF_Standalone);
    if (!Curve)
    {
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            TEXT("Failed to create color curve"), TEXT("CREATION_FAILED"));
    }

    Curve->FloatCurves[0].UpdateOrAddKey(0.0f, 1.0f);
    Curve->FloatCurves[1].UpdateOrAddKey(0.0f, 1.0f);
    Curve->FloatCurves[2].UpdateOrAddKey(0.0f, 1.0f);
    Curve->FloatCurves[3].UpdateOrAddKey(0.0f, 1.0f);
    FAssetRegistryModule::AssetCreated(Curve);
    Curve->MarkPackageDirty();
    if (!McpSafeAssetSave(Curve))
    {
        return McpFailEnvironmentAction(OutMessage, OutErrorCode,
            TEXT("Failed to save color curve"), TEXT("SAVE_FAILED"));
    }

    Resp->SetStringField(TEXT("curvePath"), Curve->GetPathName());
    McpHandlerUtils::AddVerification(Resp, Curve);
    OutMessage = TEXT("Color curve created");
    return true;
}

} // namespace McpEnvironmentHandlers

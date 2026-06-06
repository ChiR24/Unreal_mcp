#include "McpAutomationBridge_PCGHandlersPrivate.h"

#if WITH_EDITOR && MCP_HAS_PCG
namespace McpPCGHandlers
{
FString NormalizePCGSubAction(const TSharedPtr<FJsonObject>& Payload)
{
    return McpConsolidatedActions::GetPayloadSubAction(Payload);
}

FString GetFirstStringField(const TSharedPtr<FJsonObject>& Payload, std::initializer_list<const TCHAR*> Fields)
{
    if (!Payload.IsValid())
    {
        return FString();
    }

    for (const TCHAR* Field : Fields)
    {
        FString Value;
        if (Payload->TryGetStringField(Field, Value) && !Value.IsEmpty())
        {
            return Value;
        }
    }

    return FString();
}

const FPCGSettingsAlias* FindPCGSettingsAlias(const FString& RawAlias)
{
    static const FPCGSettingsAlias Aliases[] = {
        {TEXT("add_landscape_data_node"), TEXT("PCGGetLandscapeSettings")},
        {TEXT("landscape_data"), TEXT("PCGGetLandscapeSettings")},
        {TEXT("add_spline_data_node"), TEXT("PCGGetSplineSettings")},
        {TEXT("spline_data"), TEXT("PCGGetSplineSettings")},
        {TEXT("add_volume_data_node"), TEXT("PCGGetVolumeSettings")},
        {TEXT("volume_data"), TEXT("PCGGetVolumeSettings")},
        {TEXT("add_actor_data_node"), TEXT("PCGDataFromActorSettings")},
        {TEXT("actor_data"), TEXT("PCGDataFromActorSettings")},
        {TEXT("add_texture_data_node"), TEXT("PCGTextureSamplerSettings")},
        {TEXT("texture_data"), TEXT("PCGTextureSamplerSettings")},
        {TEXT("add_surface_sampler"), TEXT("PCGSurfaceSamplerSettings")},
        {TEXT("surface_sampler"), TEXT("PCGSurfaceSamplerSettings")},
        {TEXT("add_mesh_sampler"), TEXT("PCGPointFromMeshSettings")},
        {TEXT("mesh_sampler"), TEXT("PCGPointFromMeshSettings")},
        {TEXT("add_spline_sampler"), TEXT("PCGSplineSamplerSettings")},
        {TEXT("spline_sampler"), TEXT("PCGSplineSamplerSettings")},
        {TEXT("add_volume_sampler"), TEXT("PCGVolumeSamplerSettings")},
        {TEXT("volume_sampler"), TEXT("PCGVolumeSamplerSettings")},
        {TEXT("add_bounds_modifier"), TEXT("PCGBoundsModifierSettings")},
        {TEXT("bounds_modifier"), TEXT("PCGBoundsModifierSettings")},
        {TEXT("add_density_filter"), TEXT("PCGDensityFilterSettings")},
        {TEXT("density_filter"), TEXT("PCGDensityFilterSettings")},
        {TEXT("add_height_filter"), TEXT("PCGAttributeFilteringRangeSettings")},
        {TEXT("height_filter"), TEXT("PCGAttributeFilteringRangeSettings")},
        {TEXT("add_slope_filter"), TEXT("PCGNormalToDensitySettings")},
        {TEXT("slope_filter"), TEXT("PCGNormalToDensitySettings")},
        {TEXT("add_distance_filter"), TEXT("PCGDistanceSettings")},
        {TEXT("distance_filter"), TEXT("PCGDistanceSettings")},
        {TEXT("add_bounds_filter"), TEXT("PCGCullPointsOutsideActorBoundsSettings")},
        {TEXT("bounds_filter"), TEXT("PCGCullPointsOutsideActorBoundsSettings")},
        {TEXT("add_self_pruning"), TEXT("PCGSelfPruningSettings")},
        {TEXT("self_pruning"), TEXT("PCGSelfPruningSettings")},
        {TEXT("add_transform_points"), TEXT("PCGTransformPointsSettings")},
        {TEXT("transform_points"), TEXT("PCGTransformPointsSettings")},
        {TEXT("add_project_to_surface"), TEXT("PCGProjectionSettings")},
        {TEXT("project_to_surface"), TEXT("PCGProjectionSettings")},
        {TEXT("add_copy_points"), TEXT("PCGCopyPointsSettings")},
        {TEXT("copy_points"), TEXT("PCGCopyPointsSettings")},
        {TEXT("add_merge_points"), TEXT("PCGMergeSettings")},
        {TEXT("merge_points"), TEXT("PCGMergeSettings")},
        {TEXT("add_static_mesh_spawner"), TEXT("PCGStaticMeshSpawnerSettings")},
        {TEXT("static_mesh_spawner"), TEXT("PCGStaticMeshSpawnerSettings")},
        {TEXT("add_actor_spawner"), TEXT("PCGSpawnActorSettings")},
        {TEXT("actor_spawner"), TEXT("PCGSpawnActorSettings")},
        {TEXT("add_spline_spawner"), TEXT("PCGSpawnSplineSettings")},
        {TEXT("spline_spawner"), TEXT("PCGSpawnSplineSettings")}
    };

    FString Normalized = RawAlias.TrimStartAndEnd().ToLower();
    Normalized.ReplaceInline(TEXT("-"), TEXT("_"));
    Normalized.ReplaceInline(TEXT(" "), TEXT("_"));
    for (const FPCGSettingsAlias& Alias : Aliases)
    {
        if (Normalized.Equals(Alias.Alias, ESearchCase::IgnoreCase))
        {
            return &Alias;
        }
    }
    return nullptr;
}

bool IsPCGNodeCreationAction(const FString& SubAction)
{
    return SubAction.StartsWith(TEXT("add_"), ESearchCase::IgnoreCase) && FindPCGSettingsAlias(SubAction) != nullptr;
}

bool TryGetPCGAssetPath(const TSharedPtr<FJsonObject>& Payload, std::initializer_list<const TCHAR*> DirectFields, FString& OutPath, FString& OutError)
{
    OutPath = GetFirstStringField(Payload, DirectFields);
    if (OutPath.IsEmpty())
    {
        const FString Directory = GetJsonStringField(Payload, TEXT("path"), TEXT("/Game/PCG"));
        const FString Name = GetJsonStringField(Payload, TEXT("name"));
        if (!Name.IsEmpty())
        {
            OutPath = Directory / Name;
        }
    }

    if (OutPath.IsEmpty())
    {
        OutError = TEXT("Missing PCG asset path. Provide graphPath, subgraphPath, assetPath, or path + name.");
        return false;
    }

    FNormalizedAssetPath Normalized = NormalizeAssetPath(OutPath);
    if (!Normalized.bIsValid)
    {
        OutError = Normalized.ErrorMessage;
        return false;
    }

    OutPath = Normalized.Path;
    return true;
}

FString ToObjectPath(const FString& PackagePath)
{
    return FString::Printf(TEXT("%s.%s"), *PackagePath, *FPackageName::GetShortName(PackagePath));
}

UPCGGraph* LoadPCGGraph(const FString& RawPath, FString& OutPath, FString& OutError)
{
    FNormalizedAssetPath Normalized = NormalizeAssetPath(RawPath);
    if (!Normalized.bIsValid)
    {
        OutError = Normalized.ErrorMessage;
        return nullptr;
    }

    OutPath = Normalized.Path;
    UObject* Loaded = UEditorAssetLibrary::LoadAsset(OutPath);
    if (!Loaded)
    {
        Loaded = StaticLoadObject(UPCGGraph::StaticClass(), nullptr, *ToObjectPath(OutPath));
    }

    UPCGGraph* Graph = Cast<UPCGGraph>(Loaded);
    if (!Graph)
    {
        OutError = FString::Printf(TEXT("Could not load PCG graph at '%s'."), *OutPath);
    }

    return Graph;
}

UPCGGraph* CreateOrReusePCGGraph(const FString& GraphPath, bool bOverwrite, bool bSave, bool& bOutCreated, bool& bOutSaved, FString& OutError)
{
    bOutCreated = false;
    bOutSaved = false;

    if (UEditorAssetLibrary::DoesAssetExist(GraphPath))
    {
        FString LoadedPath;
        UPCGGraph* Existing = LoadPCGGraph(GraphPath, LoadedPath, OutError);
        if (!Existing)
        {
            return nullptr;
        }
        if (!bOverwrite)
        {
            OutError = FString::Printf(TEXT("PCG graph already exists at '%s'."), *GraphPath);
            return nullptr;
        }
        return Existing;
    }

    const FString AssetName = FPackageName::GetShortName(GraphPath);
    UPackage* Package = CreatePackage(*GraphPath);
    if (!Package)
    {
        OutError = FString::Printf(TEXT("Failed to create package '%s'."), *GraphPath);
        return nullptr;
    }

    UPCGGraph* Graph = NewObject<UPCGGraph>(Package, *AssetName, RF_Public | RF_Standalone | RF_Transactional);
    if (!Graph)
    {
        OutError = FString::Printf(TEXT("Failed to create PCG graph '%s'."), *GraphPath);
        return nullptr;
    }

    Graph->MarkPackageDirty();
    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Graph);
    bOutCreated = true;

    if (bSave)
    {
        bOutSaved = McpSafeAssetSave(Graph);
        if (!bOutSaved)
        {
            OutError = FString::Printf(TEXT("Created PCG graph '%s' but failed to save it."), *GraphPath);
            return nullptr;
        }
    }

    return Graph;
}

TSharedPtr<FJsonObject> BuildGraphResult(UPCGGraph* Graph, const FString& GraphPath, bool bCreated, bool bSaved)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("graphPath"), GraphPath);
    Result->SetStringField(TEXT("assetPath"), GraphPath);
    Result->SetStringField(TEXT("name"), FPackageName::GetShortName(GraphPath));
    Result->SetBoolField(TEXT("created"), bCreated);
    Result->SetBoolField(TEXT("saved"), bSaved);
    McpHandlerUtils::AddVerification(Result, Graph);
    return Result;
}
}
#endif

#include "Domains/PCG/McpAutomationBridge_PCGHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintPaths.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
const TCHAR* FindPCGSettingsAlias(const FString& RawAlias)
{
    // Node kind -> settings class. "add_<kind>" and "add_<kind>_node" name the same kind as "<kind>".
    static const TMap<FString, const TCHAR*> Kinds = {
        {TEXT("landscape_data"), TEXT("PCGGetLandscapeSettings")},
        {TEXT("spline_data"), TEXT("PCGGetSplineSettings")},
        {TEXT("volume_data"), TEXT("PCGGetVolumeSettings")},
        {TEXT("actor_data"), TEXT("PCGDataFromActorSettings")},
        {TEXT("texture_data"), TEXT("PCGTextureSamplerSettings")},
        {TEXT("surface_sampler"), TEXT("PCGSurfaceSamplerSettings")},
        {TEXT("mesh_sampler"), TEXT("PCGPointFromMeshSettings")},
        {TEXT("spline_sampler"), TEXT("PCGSplineSamplerSettings")},
        {TEXT("volume_sampler"), TEXT("PCGVolumeSamplerSettings")},
        {TEXT("bounds_modifier"), TEXT("PCGBoundsModifierSettings")},
        {TEXT("density_filter"), TEXT("PCGDensityFilterSettings")},
        {TEXT("height_filter"), TEXT("PCGAttributeFilteringRangeSettings")},
        {TEXT("slope_filter"), TEXT("PCGNormalToDensitySettings")},
        {TEXT("distance_filter"), TEXT("PCGDistanceSettings")},
        {TEXT("bounds_filter"), TEXT("PCGCullPointsOutsideActorBoundsSettings")},
        {TEXT("self_pruning"), TEXT("PCGSelfPruningSettings")},
        {TEXT("transform_points"), TEXT("PCGTransformPointsSettings")},
        {TEXT("project_to_surface"), TEXT("PCGProjectionSettings")},
        {TEXT("copy_points"), TEXT("PCGCopyPointsSettings")},
        {TEXT("merge_points"), TEXT("PCGMergeSettings")},
        {TEXT("static_mesh_spawner"), TEXT("PCGStaticMeshSpawnerSettings")},
        {TEXT("actor_spawner"), TEXT("PCGSpawnActorSettings")},
        {TEXT("spline_spawner"), TEXT("PCGSpawnSplineSettings")},
    };
    FString Kind = RawAlias.TrimStartAndEnd().ToLower();
    Kind.ReplaceInline(TEXT("-"), TEXT("_"));
    Kind.ReplaceInline(TEXT(" "), TEXT("_"));
    Kind.RemoveFromStart(TEXT("add_"));
    Kind.RemoveFromEnd(TEXT("_node"));
    const TCHAR* const* SettingsClass = Kinds.Find(Kind);
    return SettingsClass ? *SettingsClass : nullptr;
}

bool IsPCGNodeCreationAction(const FString& SubAction)
{
    return SubAction.StartsWith(TEXT("add_"), ESearchCase::IgnoreCase) && FindPCGSettingsAlias(SubAction) != nullptr;
}

bool TryGetPCGAssetPath(const TSharedPtr<FJsonObject>& Payload, std::initializer_list<const TCHAR*> DirectFields, FString& OutPath, FString& OutError)
{
    OutPath = McpGetFirstStringField(Payload, DirectFields);
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

UPCGGraph* CreateOrReusePCGGraph(const FString& GraphPath, bool bOverwrite, bool bSave, bool& bOutCreated, bool& bOutSaved, FString& OutError)
{
    bOutCreated = false;
    bOutSaved = false;

    if (McpAssetExists(GraphPath))
    {
        FString LoadedPath;
        UPCGGraph* Existing = LoadPCGAsset<UPCGGraph>(GraphPath, TEXT("PCG graph"), LoadedPath, OutError);
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

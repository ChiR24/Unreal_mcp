#include "Domains/LevelStructure/McpLevelStructureActions.h"
#include "Foundation/BridgeHelpers/Security/McpAssetPathCanonical.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Domains/LevelStructure/McpLevelStructureEditorWorld.h"
#include "Domains/Volume/McpVolumeGeometry.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "GameFramework/Volume.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "UObject/Package.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/WorldPartition.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "WorldPartition/WorldPartitionMiniMapVolume.h"
#endif

namespace McpLevelStructure
{

bool HandleConfigureHlodLayer(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    // CRITICAL: hlodLayerName is required - no default fallback
    const FString HlodLayerName = GetJsonStringField(Payload, TEXT("hlodLayerName"));

    if (HlodLayerName.IsEmpty())
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("hlodLayerName is required for configure_hlod_layer"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString HlodLayerPath = GetJsonStringField(Payload, TEXT("hlodLayerPath"), TEXT("/Game/HLOD"));
    bool bIsSpatiallyLoaded = GetJsonBoolField(Payload, TEXT("bIsSpatiallyLoaded"), true);
    int32 CellSize = GetJsonIntField(Payload, TEXT("cellSize"), 25600);
    double LoadingDistance = GetJsonNumberField(Payload, TEXT("loadingDistance"), 51200.0);
    FString LayerType = GetJsonStringField(Payload, TEXT("layerType"), TEXT("MeshMerge"));

    // Security: Validate HLOD layer path format to prevent traversal attacks
    FString SafePath = SanitizeProjectRelativePath(HlodLayerPath);
    if (SafePath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            McpPathRefusalMessage(TEXT("HLOD layer path"), HlodLayerPath),
            nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }
    HlodLayerPath = SafePath;

    // The name is joined to the folder only when it is one an asset can take: it was joined unchecked and named the
    // object, so a separator made a subfolder and a dot an object no path resolves.
    FString FullPath, PathError, PathCode;
    if (!McpBuildValidatedAssetPath(HlodLayerPath, HlodLayerName, TEXT("HLOD layer"), FullPath, PathError, PathCode))
    {
        Subsystem->SendAutomationError(Socket, RequestId, PathError, PathCode);
        return true;
    }

    // An unknown layerType was written as nothing and answered success with layerTypeApplied false: refused before
    // the asset exists.
    static const TMap<FString, FString> LayerTypeAliases = {
        {TEXT("SimplifiedMesh"), TEXT("MeshSimplify")}, {TEXT("ApproximatedMesh"), TEXT("MeshApproximate")}};
    const FString* LayerTypeAlias = LayerTypeAliases.Find(LayerType);
    const FString ResolvedLayerType = LayerTypeAlias ? *LayerTypeAlias : LayerType;
    if (StaticEnum<EHLODLayerType>()->GetValueByNameString(ResolvedLayerType) == INDEX_NONE)
    {
        Subsystem->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("layerType '%s' is not an HLOD layer type: use "
            "Instancing, MeshMerge, MeshSimplify or MeshApproximate. Nothing was created."), *LayerType), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString CreateError, CreateCode;
    UPackage* AssetPackage = McpCreatePackageForNewAsset(FullPath, CreateError, CreateCode);
    if (!AssetPackage)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            CreateError, nullptr, CreateCode);
        return true;
    }

    UHLODLayer* NewHLODLayer = NewObject<UHLODLayer>(AssetPackage, *HlodLayerName, RF_Public | RF_Standalone | RF_Transactional);

    // LayerType, bIsSpatiallyLoaded, CellSize and LoadingRange are reflected properties on every 5.x UHLODLayer
    // (5.7 moved only their public setters), so each is written through reflection and reported as applied or not.
    auto ApplyLayerProperty = [NewHLODLayer](const TCHAR* Name, const TSharedPtr<FJsonValue>& Value)
    {
        FProperty* Property = NewHLODLayer->GetClass()->FindPropertyByName(Name);
        FString ApplyError;
        return Property && ApplyJsonValueToProperty(NewHLODLayer, Property, Value, ApplyError);
    };
    const bool bLayerTypeApplied = ApplyLayerProperty(TEXT("LayerType"), MakeShared<FJsonValueString>(ResolvedLayerType));
    const bool bSpatiallyLoadedApplied = ApplyLayerProperty(TEXT("bIsSpatiallyLoaded"), MakeShared<FJsonValueBoolean>(bIsSpatiallyLoaded));
    const bool bCellSizeApplied = ApplyLayerProperty(TEXT("CellSize"), MakeShared<FJsonValueNumber>(CellSize));
    const bool bLoadingDistanceApplied = ApplyLayerProperty(TEXT("LoadingRange"), MakeShared<FJsonValueNumber>(LoadingDistance));

    AssetPackage->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewHLODLayer);

    // save (default true) persists the new layer asset; a failed save is reported, not hidden.
    if (GetJsonBoolField(Payload, TEXT("save"), true) && !McpSafeAssetSave(NewHLODLayer))
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("HLOD layer created but saving %s failed; it exists only in this editor session."), *FullPath), TEXT("SAVE_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("hlodLayerName"), HlodLayerName);
    ResponseJson->SetStringField(TEXT("hlodLayerPath"), FullPath);
    ResponseJson->SetBoolField(TEXT("isSpatiallyLoaded"), bIsSpatiallyLoaded);
    ResponseJson->SetNumberField(TEXT("cellSize"), CellSize);
    ResponseJson->SetNumberField(TEXT("loadingDistance"), LoadingDistance);
    ResponseJson->SetStringField(TEXT("layerType"), LayerType);
    ResponseJson->SetBoolField(TEXT("layerTypeApplied"), bLayerTypeApplied);
    ResponseJson->SetBoolField(TEXT("isSpatiallyLoadedApplied"), bSpatiallyLoadedApplied);
    ResponseJson->SetBoolField(TEXT("cellSizeApplied"), bCellSizeApplied);
    ResponseJson->SetBoolField(TEXT("loadingDistanceApplied"), bLoadingDistanceApplied);

    FString Message = FString::Printf(TEXT("Created HLOD layer '%s' at '%s'"),
        *HlodLayerName, *FullPath);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

bool HandleCreateMinimapVolume(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    using namespace LevelStructureHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("MinimapVolume"));
    // location and extent are the declared names; volumeLocation and volumeExtent are older spellings.
    FVector VolumeLocation = ExtractVectorField(Payload, TEXT("location"), ExtractVectorField(Payload, TEXT("volumeLocation"), FVector::ZeroVector));
    FVector VolumeExtent = ExtractVectorField(Payload, TEXT("extent"), ExtractVectorField(Payload, TEXT("volumeExtent"), FVector(10000.0)));

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("No editor world available"), TEXT("NO_EDITOR_WORLD"));
        return true;
    }

    // Check if World Partition is enabled (minimap volume is for WP)
    UWorldPartition* WorldPartition = World->GetWorldPartition();
    if (!WorldPartition)
    {
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("World Partition is not enabled. AWorldPartitionMiniMapVolume requires World Partition."), TEXT("WORLD_PARTITION_NOT_ENABLED"));
        return true;
    }

    // Spawned, labelled and given the box brush the editor gives a placed volume: a volume from SpawnActor has none, so
    // scaling it left bounds of zero and the minimap builder, which captures the volume's bounds, drew nothing.
    // One undo step for the placement, closed before the level is saved.
    TUniquePtr<FMcpScopedEditorTransaction> SpawnStep = McpBeginSpawnTransaction(RequestId);
    AWorldPartitionMiniMapVolume* MiniMapVolume = VolumeHelpers::SpawnVolumeActor<AWorldPartitionMiniMapVolume>(
        World, VolumeName, VolumeLocation, FRotator::ZeroRotator, VolumeExtent);

    if (!MiniMapVolume)
    {
        if (SpawnStep) SpawnStep->Cancel(); // nothing was placed
        Subsystem->SendAutomationError(Socket, RequestId,
            TEXT("Failed to spawn AWorldPartitionMiniMapVolume actor"), TEXT("ACTOR_SPAWN_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, MiniMapVolume);
    ResponseJson->SetStringField(TEXT("volumeName"), VolumeName);
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("AWorldPartitionMiniMapVolume"));

    ResponseJson->SetObjectField(TEXT("volumeLocation"), McpHandlerUtils::VectorToJson(VolumeLocation));

    ResponseJson->SetObjectField(TEXT("volumeExtent"), McpHandlerUtils::VectorToJson(VolumeExtent));

    FString Message = FString::Printf(TEXT("Created minimap volume '%s' at (%f, %f, %f)"),
        *VolumeName, VolumeLocation.X, VolumeLocation.Y, VolumeLocation.Z);
    SpawnStep.Reset();
    SendLevelEditResult(Subsystem, RequestId, Socket, Payload, MiniMapVolume->GetLevel(), Message, ResponseJson);
#else
    Subsystem->SendAutomationError(Socket, RequestId,
        TEXT("Minimap volume requires Unreal Engine 5.1 or later."), TEXT("UNSUPPORTED_VERSION"));
#endif
    return true;
}

}

#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureActions.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "GameFramework/Volume.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
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
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    // CRITICAL: hlodLayerName is required - no default fallback
    FString HlodLayerName;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("hlodLayerName"), HlodLayerName);
    }

    if (HlodLayerName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("hlodLayerName is required for configure_hlod_layer"), nullptr, TEXT("INVALID_ARGUMENT"));
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

    FString FullPath = HlodLayerPath / HlodLayerName;
    if (!FullPath.StartsWith(TEXT("/")))
    {
        FullPath = TEXT("/Game/") + FullPath;
    }

    UPackage* AssetPackage = CreatePackage(*FullPath);
    if (!AssetPackage)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create package for HLOD layer at: %s"), *FullPath), nullptr, TEXT("PACKAGE_CREATION_FAILED"));
        return true;
    }

    UHLODLayer* NewHLODLayer = NewObject<UHLODLayer>(AssetPackage, *HlodLayerName, RF_Public | RF_Standalone);
    if (!NewHLODLayer)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create UHLODLayer object"), nullptr, TEXT("ASSET_CREATION_FAILED"));
        return true;
    }

    // LayerType, bIsSpatiallyLoaded, CellSize and LoadingRange are reflected properties on every 5.x UHLODLayer
    // (5.7 moved only their public setters), so each is written through reflection and reported as applied or not.
    static const TMap<FString, FString> LayerTypeAliases = {
        {TEXT("SimplifiedMesh"), TEXT("MeshSimplify")}, {TEXT("ApproximatedMesh"), TEXT("MeshApproximate")}};
    const FString* LayerTypeAlias = LayerTypeAliases.Find(LayerType);
    auto ApplyLayerProperty = [NewHLODLayer](const TCHAR* Name, const TSharedPtr<FJsonValue>& Value)
    {
        FProperty* Property = NewHLODLayer->GetClass()->FindPropertyByName(Name);
        FString ApplyError;
        return Property && ApplyJsonValueToProperty(NewHLODLayer, Property, Value, ApplyError);
    };
    const bool bLayerTypeApplied = ApplyLayerProperty(TEXT("LayerType"), MakeShared<FJsonValueString>(LayerTypeAlias ? *LayerTypeAlias : LayerType));
    const bool bSpatiallyLoadedApplied = ApplyLayerProperty(TEXT("bIsSpatiallyLoaded"), MakeShared<FJsonValueBoolean>(bIsSpatiallyLoaded));
    const bool bCellSizeApplied = ApplyLayerProperty(TEXT("CellSize"), MakeShared<FJsonValueNumber>(CellSize));
    const bool bLoadingDistanceApplied = ApplyLayerProperty(TEXT("LoadingRange"), MakeShared<FJsonValueNumber>(LoadingDistance));

    AssetPackage->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewHLODLayer);

    // save (default true) persists the new layer asset; a failed save is reported, not hidden.
    if (GetJsonBoolField(Payload, TEXT("save"), true) && !McpSafeAssetSave(NewHLODLayer))
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("HLOD layer created but saving %s failed; it exists only in this editor session."), *FullPath), nullptr, TEXT("SAVE_FAILED"));
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
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if ENGINE_MINOR_VERSION >= 1
    using namespace LevelStructureHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("MinimapVolume"));
    // location and extent are the declared names; volumeLocation and volumeExtent are older spellings.
    FVector VolumeLocation = ExtractVectorField(Payload, TEXT("location"), ExtractVectorField(Payload, TEXT("volumeLocation"), FVector::ZeroVector));
    FVector VolumeExtent = ExtractVectorField(Payload, TEXT("extent"), ExtractVectorField(Payload, TEXT("volumeExtent"), FVector(10000.0)));

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Check if World Partition is enabled (minimap volume is for WP)
    UWorldPartition* WorldPartition = World->GetWorldPartition();
    if (!WorldPartition)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("World Partition is not enabled. AWorldPartitionMiniMapVolume requires World Partition."), nullptr, TEXT("WORLD_PARTITION_NOT_ENABLED"));
        return true;
    }

    // Spawn the AWorldPartitionMiniMapVolume
    FActorSpawnParameters SpawnParams;
    // CRITICAL FIX: Use MakeUniqueObjectName to prevent "Cannot generate unique name" crash
    // This prevents fatal error when multiple volumes with same name are created
    SpawnParams.Name = MakeUniqueObjectName(World, AWorldPartitionMiniMapVolume::StaticClass(), FName(*VolumeName));
    SpawnParams.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;  // Auto-generate unique name if still taken
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AWorldPartitionMiniMapVolume* MiniMapVolume = World->SpawnActor<AWorldPartitionMiniMapVolume>(
        AWorldPartitionMiniMapVolume::StaticClass(),
        VolumeLocation,
        FRotator::ZeroRotator,
        SpawnParams
    );

    if (!MiniMapVolume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn AWorldPartitionMiniMapVolume actor"), nullptr, TEXT("ACTOR_SPAWN_FAILED"));
        return true;
    }

    // Set actor label to the requested name (may differ from internal name if collision occurred)
    MiniMapVolume->SetActorLabel(*VolumeName);

    // Scale the volume to match the extent (AVolume uses a brush, scale affects it)
    // The default brush is a 200x200x200 cube, so we scale it to match the desired extent
    FVector DesiredScale = VolumeExtent / 100.0; // Brush is 200 units, so divide by half
    MiniMapVolume->SetActorScale3D(DesiredScale);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, MiniMapVolume);
    ResponseJson->SetStringField(TEXT("volumeName"), VolumeName);
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("AWorldPartitionMiniMapVolume"));

    ResponseJson->SetObjectField(TEXT("volumeLocation"), McpHandlerUtils::VectorToJson(VolumeLocation));

    ResponseJson->SetObjectField(TEXT("volumeExtent"), McpHandlerUtils::VectorToJson(VolumeExtent));

    FString Message = FString::Printf(TEXT("Created minimap volume '%s' at (%f, %f, %f)"),
        *VolumeName, VolumeLocation.X, VolumeLocation.Y, VolumeLocation.Z);
    SendLevelEditResult(Subsystem, RequestId, Socket, Payload, MiniMapVolume->GetLevel(), Message, ResponseJson);
#else
    Subsystem->SendAutomationResponse(Socket, RequestId, false,
        TEXT("Minimap volume requires Unreal Engine 5.1 or later."), nullptr);
#endif
    return true;
}

}

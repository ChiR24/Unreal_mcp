#include "McpAutomationBridge_LevelStructureActions.h"
#include "McpAutomationBridge_LevelStructureEditorWorld.h"
#include "McpAutomationBridge_LevelStructurePayload.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/World.h"
#include "GameFramework/Volume.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpBridgeWebSocket.h"
#include "McpHandlerUtils.h"
#include "UObject/Package.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/WorldPartition.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "WorldPartition/WorldPartitionMiniMapVolume.h"
#endif

#if WITH_EDITOR
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
            FString::Printf(TEXT("Invalid or unsafe HLOD layer path: %s"), *HlodLayerPath),
            nullptr, TEXT("SECURITY_VIOLATION"));
        return true;
    }
    HlodLayerPath = SafePath;

    // Build full path
    FString FullPath = HlodLayerPath / HlodLayerName;
    if (!FullPath.StartsWith(TEXT("/")))
    {
        FullPath = TEXT("/Game/") + FullPath;
    }

    // Create the package for the HLOD layer asset
    UPackage* AssetPackage = CreatePackage(*FullPath);
    if (!AssetPackage)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create package for HLOD layer at: %s"), *FullPath), nullptr, TEXT("PACKAGE_CREATION_FAILED"));
        return true;
    }

    // Create the UHLODLayer asset
    UHLODLayer* NewHLODLayer = NewObject<UHLODLayer>(AssetPackage, *HlodLayerName, RF_Public | RF_Standalone);
    if (!NewHLODLayer)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create UHLODLayer object"), nullptr, TEXT("ASSET_CREATION_FAILED"));
        return true;
    }

    // Configure the HLOD layer
    // UE 5.1-5.6: SetIsSpatiallyLoaded is available
    // UE 5.7+: Deprecated - streaming grid properties are in partition settings
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1 && ENGINE_MINOR_VERSION < 7
    NewHLODLayer->SetIsSpatiallyLoaded(bIsSpatiallyLoaded);

    // Set layer type
    if (LayerType == TEXT("Instancing"))
    {
        NewHLODLayer->SetLayerType(EHLODLayerType::Instancing);
    }
    else if (LayerType == TEXT("MeshSimplify") || LayerType == TEXT("SimplifiedMesh"))
    {
        NewHLODLayer->SetLayerType(EHLODLayerType::MeshSimplify);
    }
    else if (LayerType == TEXT("MeshApproximate") || LayerType == TEXT("ApproximatedMesh"))
    {
        NewHLODLayer->SetLayerType(EHLODLayerType::MeshApproximate);
    }
    else // Default to MeshMerge
    {
        NewHLODLayer->SetLayerType(EHLODLayerType::MeshMerge);
    }
#endif

    // Mark package dirty and notify asset registry
    AssetPackage->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewHLODLayer);

    // Save the asset
    McpSafeAssetSave(NewHLODLayer);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetStringField(TEXT("hlodLayerName"), HlodLayerName);
    ResponseJson->SetStringField(TEXT("hlodLayerPath"), FullPath);
    ResponseJson->SetBoolField(TEXT("isSpatiallyLoaded"), bIsSpatiallyLoaded);
    ResponseJson->SetNumberField(TEXT("cellSize"), CellSize);
    ResponseJson->SetNumberField(TEXT("loadingDistance"), LoadingDistance);
    ResponseJson->SetStringField(TEXT("layerType"), LayerType);

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
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
    using namespace LevelStructureHelpers;

    FString VolumeName = GetJsonStringField(Payload, TEXT("volumeName"), TEXT("MinimapVolume"));
    FVector VolumeLocation = LevelStructureHelpers::GetVectorFromJson(GetObjectField(Payload, TEXT("volumeLocation")));
    FVector VolumeExtent = LevelStructureHelpers::GetVectorFromJson(GetObjectField(Payload, TEXT("volumeExtent")), FVector(10000.0));

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
    FVector CurrentScale = MiniMapVolume->GetActorScale3D();
    FVector DesiredScale = VolumeExtent / 100.0; // Brush is 200 units, so divide by half
    MiniMapVolume->SetActorScale3D(DesiredScale);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, MiniMapVolume);
    ResponseJson->SetStringField(TEXT("volumeName"), VolumeName);
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("AWorldPartitionMiniMapVolume"));

    TSharedPtr<FJsonObject> LocationJson = McpHandlerUtils::CreateResultObject();
    LocationJson->SetNumberField(TEXT("x"), VolumeLocation.X);
    LocationJson->SetNumberField(TEXT("y"), VolumeLocation.Y);
    LocationJson->SetNumberField(TEXT("z"), VolumeLocation.Z);
    ResponseJson->SetObjectField(TEXT("volumeLocation"), LocationJson);

    TSharedPtr<FJsonObject> ExtentJson = McpHandlerUtils::CreateResultObject();
    ExtentJson->SetNumberField(TEXT("x"), VolumeExtent.X);
    ExtentJson->SetNumberField(TEXT("y"), VolumeExtent.Y);
    ExtentJson->SetNumberField(TEXT("z"), VolumeExtent.Z);
    ResponseJson->SetObjectField(TEXT("volumeExtent"), ExtentJson);

    FString Message = FString::Printf(TEXT("Created minimap volume '%s' at (%f, %f, %f)"),
        *VolumeName, VolumeLocation.X, VolumeLocation.Y, VolumeLocation.Z);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
#else
    Subsystem->SendAutomationResponse(Socket, RequestId, false,
        TEXT("Minimap volume requires Unreal Engine 5.1 or later."), nullptr);
#endif
    return true;
}

// ============================================================================
// Level Blueprint Handlers (3 actions)
// ============================================================================

}
#endif

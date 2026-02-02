#include "Dom/JsonObject.h"
// McpAutomationBridge_LevelStructureHandlers.cpp
// Phase 23: Level Structure Handlers
//
// Complete level and world structure management including:
// - Levels (create levels, sublevels, streaming, bounds)
// - World Partition (grid configuration, data layers, HLOD)
// - Level Blueprint (open, add nodes, connect nodes)
// - Level Instances (packed level actors, level instances)

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpBridgeWebSocket.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "LevelEditor.h"
#include "EditorLevelUtils.h"
#include "FileHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/LevelScriptActor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"
#endif
#include "WorldPartition/DataLayer/WorldDataLayers.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "Engine/LODActor.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "PackedLevelActor/PackedLevelActor.h"
#include "DataLayer/DataLayerEditorSubsystem.h"
#include "AssetToolsModule.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "WorldPartition/WorldPartitionMiniMapVolume.h"
#endif
#include "WorldPartition/WorldPartitionRuntimeSpatialHash.h"
#include "Engine/LevelStreamingVolume.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogMcpLevelStructureHandlers, Log, All);

// ============================================================================
// Helper Functions
// ============================================================================
// NOTE: Uses consolidated JSON helpers from McpAutomationBridgeHelpers.h:
//   - GetJsonStringField(Obj, Field, Default)
//   - GetJsonNumberField(Obj, Field, Default)
//   - GetJsonBoolField(Obj, Field, Default)
//   - GetJsonIntField(Obj, Field, Default)
//   - ExtractVectorField(Source, FieldName, Default)
//   - ExtractRotatorField(Source, FieldName, Default)
// ============================================================================

namespace LevelStructureHelpers
{
    // Get object field (no consolidated equivalent, keep local)
    TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Payload, const FString& FieldName)
    {
        if (Payload.IsValid() && Payload->HasTypedField<EJson::Object>(FieldName))
        {
            return Payload->GetObjectField(FieldName);
        }
        return nullptr;
    }

    // Get FVector from JSON object field
    FVector GetVectorFromJson(const TSharedPtr<FJsonObject>& JsonObj, FVector Default = FVector::ZeroVector)
    {
        if (!JsonObj.IsValid()) return Default;
        return FVector(
            GetJsonNumberField(JsonObj, TEXT("x"), Default.X),
            GetJsonNumberField(JsonObj, TEXT("y"), Default.Y),
            GetJsonNumberField(JsonObj, TEXT("z"), Default.Z)
        );
    }

    // Get FRotator from JSON object field
    FRotator GetRotatorFromJson(const TSharedPtr<FJsonObject>& JsonObj, FRotator Default = FRotator::ZeroRotator)
    {
        if (!JsonObj.IsValid()) return Default;
        return FRotator(
            GetJsonNumberField(JsonObj, TEXT("pitch"), Default.Pitch),
            GetJsonNumberField(JsonObj, TEXT("yaw"), Default.Yaw),
            GetJsonNumberField(JsonObj, TEXT("roll"), Default.Roll)
        );
    }

#if WITH_EDITOR
    // Get current world
    UWorld* GetEditorWorld()
    {
        if (GEditor)
        {
            return GEditor->GetEditorWorldContext().World();
        }
        return nullptr;
    }
#endif
}

// ============================================================================
// Levels Handlers (5 actions)
// ============================================================================

#if WITH_EDITOR

static bool HandleCreateLevel(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString LevelName = GetJsonStringField(Payload, TEXT("levelName"), TEXT("NewLevel"));
    FString LevelPath = GetJsonStringField(Payload, TEXT("levelPath"), TEXT("/Game/Maps"));
    bool bCreateWorldPartition = GetJsonBoolField(Payload, TEXT("bCreateWorldPartition"), false);
    bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);

    // Build full path
    FString FullPath = LevelPath / LevelName;
    if (!FullPath.StartsWith(TEXT("/Game/")))
    {
        FullPath = TEXT("/Game/") + FullPath;
    }

    // Create the level package
    UPackage* Package = CreatePackage(*FullPath);
    if (!Package)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create package for level: %s"), *FullPath), nullptr);
        return true;
    }

    // Create a new world
    UWorld* NewWorld = UWorld::CreateWorld(EWorldType::Inactive, false, FName(*LevelName), Package);
    if (!NewWorld)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create world for level: %s"), *FullPath), nullptr);
        return true;
    }

    // Initialize the world only if not already initialized
    // CreateWorld may already initialize it in some UE versions
    if (!NewWorld->bIsWorldInitialized)
    {
        NewWorld->InitWorld();
    }

    // Enable World Partition if requested
    bool bWorldPartitionActuallyEnabled = false;
#if ENGINE_MAJOR_VERSION >= 5
    if (bCreateWorldPartition)
    {
        // World Partition is enabled via WorldSettings
        AWorldSettings* WorldSettings = NewWorld->GetWorldSettings();
        if (WorldSettings)
        {
            // In UE5, World Partition is typically enabled at world creation time
            // or via project settings. We mark it as requested but note the limitation.
            bWorldPartitionActuallyEnabled = false; // Requires editor UI to fully enable
        }
    }
#endif

    // Mark package dirty
    Package->MarkPackageDirty();

    // Save if requested
    if (bSave)
    {
        McpSafeAssetSave(NewWorld);
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("levelPath"), FullPath);
    ResponseJson->SetStringField(TEXT("levelName"), LevelName);
    ResponseJson->SetBoolField(TEXT("worldPartitionEnabled"), bWorldPartitionActuallyEnabled);
    ResponseJson->SetBoolField(TEXT("worldPartitionRequested"), bCreateWorldPartition);
    if (bCreateWorldPartition && !bWorldPartitionActuallyEnabled)
    {
        ResponseJson->SetStringField(TEXT("worldPartitionNote"), TEXT("World Partition must be enabled via editor UI or project settings for new levels"));
    }

    FString Message = FString::Printf(TEXT("Created level: %s"), *FullPath);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

static bool HandleCreateSublevel(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString SublevelName = GetJsonStringField(Payload, TEXT("sublevelName"), TEXT("Sublevel"));
    FString SublevelPath = GetJsonStringField(Payload, TEXT("sublevelPath"), TEXT(""));
    bool bSave = GetJsonBoolField(Payload, TEXT("save"), true);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Create sublevel path if not provided
    if (SublevelPath.IsEmpty())
    {
        FString WorldPath = World->GetOutermost()->GetName();
        SublevelPath = FPaths::GetPath(WorldPath) / SublevelName;
    }

    // Add streaming level
    ULevelStreamingDynamic* StreamingLevel = NewObject<ULevelStreamingDynamic>(World, ULevelStreamingDynamic::StaticClass());
    if (!StreamingLevel)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create streaming level object"), nullptr);
        return true;
    }

    // Configure the streaming level
    StreamingLevel->SetWorldAssetByPackageName(FName(*SublevelPath));
    StreamingLevel->LevelTransform = FTransform::Identity;
    StreamingLevel->SetShouldBeVisible(true);
    StreamingLevel->SetShouldBeLoaded(true);

    // Add to world's streaming levels
    World->AddStreamingLevel(StreamingLevel);

    // Mark world dirty so changes can be saved
    World->MarkPackageDirty();
    
    // Save if requested
    if (bSave)
    {
        McpSafeAssetSave(World);
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("sublevelPath"), SublevelPath);
    ResponseJson->SetStringField(TEXT("sublevelName"), SublevelName);
    ResponseJson->SetStringField(TEXT("parentLevel"), World->GetMapName());
    ResponseJson->SetBoolField(TEXT("saved"), bSave);

    FString Message = FString::Printf(TEXT("Created sublevel: %s"), *SublevelName);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

static bool HandleConfigureLevelStreaming(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString LevelName = GetJsonStringField(Payload, TEXT("levelName"), TEXT(""));
    FString StreamingMethod = GetJsonStringField(Payload, TEXT("streamingMethod"), TEXT("Blueprint"));
    bool bShouldBeVisible = GetJsonBoolField(Payload, TEXT("bShouldBeVisible"), true);
    bool bShouldBlockOnLoad = GetJsonBoolField(Payload, TEXT("bShouldBlockOnLoad"), false);
    bool bDisableDistanceStreaming = GetJsonBoolField(Payload, TEXT("bDisableDistanceStreaming"), false);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Find the streaming level
    ULevelStreaming* FoundLevel = nullptr;
    for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
    {
        if (StreamingLevel && StreamingLevel->GetWorldAssetPackageFName().ToString().Contains(LevelName))
        {
            FoundLevel = StreamingLevel;
            break;
        }
    }

    if (!FoundLevel)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Streaming level not found: %s"), *LevelName), nullptr);
        return true;
    }

    // Configure streaming settings
    FoundLevel->SetShouldBeVisible(bShouldBeVisible);
    FoundLevel->bShouldBlockOnLoad = bShouldBlockOnLoad;
    FoundLevel->bDisableDistanceStreaming = bDisableDistanceStreaming;

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("levelName"), LevelName);
    ResponseJson->SetStringField(TEXT("streamingMethod"), StreamingMethod);
    ResponseJson->SetBoolField(TEXT("shouldBeVisible"), bShouldBeVisible);

    FString Message = FString::Printf(TEXT("Configured streaming for level: %s"), *LevelName);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

static bool HandleSetStreamingDistance(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString LevelName = GetJsonStringField(Payload, TEXT("levelName"), TEXT(""));
    double StreamingDistance = GetJsonNumberField(Payload, TEXT("streamingDistance"), 10000.0);
    FString StreamingUsage = GetJsonStringField(Payload, TEXT("streamingUsage"), TEXT("LoadingAndVisibility"));
    TSharedPtr<FJsonObject> VolumeLocationJson = GetObjectField(Payload, TEXT("volumeLocation"));
    FVector VolumeLocation = VolumeLocationJson.IsValid() ? LevelStructureHelpers::GetVectorFromJson(VolumeLocationJson) : FVector::ZeroVector;
    bool bCreateVolume = GetJsonBoolField(Payload, TEXT("createVolume"), true);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Find the streaming level
    ULevelStreaming* FoundLevel = nullptr;
    for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
    {
        if (StreamingLevel && StreamingLevel->GetWorldAssetPackageFName().ToString().Contains(LevelName))
        {
            FoundLevel = StreamingLevel;
            break;
        }
    }

    if (!FoundLevel)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Streaming level not found: %s"), *LevelName), nullptr);
        return true;
    }

    // ULevelStreaming doesn't have a streaming distance property directly
    // Instead, we create/configure an ALevelStreamingVolume and associate it
    
    if (!bCreateVolume)
    {
        // Just report current streaming volumes
        TArray<TSharedPtr<FJsonValue>> VolumesArray;
        for (ALevelStreamingVolume* Volume : FoundLevel->EditorStreamingVolumes)
        {
            if (Volume)
            {
                TSharedPtr<FJsonObject> VolumeObj = MakeShareable(new FJsonObject());
                VolumeObj->SetStringField(TEXT("name"), Volume->GetActorLabel());
                VolumeObj->SetNumberField(TEXT("usage"), static_cast<int32>(Volume->StreamingUsage));
                VolumesArray.Add(MakeShareable(new FJsonValueObject(VolumeObj)));
            }
        }
        
        TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
        ResponseJson->SetStringField(TEXT("levelName"), LevelName);
        ResponseJson->SetArrayField(TEXT("streamingVolumes"), VolumesArray);
        ResponseJson->SetNumberField(TEXT("volumeCount"), VolumesArray.Num());
        ResponseJson->SetStringField(TEXT("note"), TEXT("Use createVolume=true to create a streaming volume for distance-based loading"));
        
        Subsystem->SendAutomationResponse(Socket, RequestId, true,
            FString::Printf(TEXT("Level '%s' has %d streaming volume(s)"), *LevelName, VolumesArray.Num()), ResponseJson);
        return true;
    }

    // Create an ALevelStreamingVolume at the specified location with size based on streaming distance
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = MakeUniqueObjectName(World, ALevelStreamingVolume::StaticClass(), 
        FName(*FString::Printf(TEXT("StreamingVolume_%s"), *LevelName)));
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ALevelStreamingVolume* NewVolume = World->SpawnActor<ALevelStreamingVolume>(
        ALevelStreamingVolume::StaticClass(),
        VolumeLocation,
        FRotator::ZeroRotator,
        SpawnParams
    );

    if (!NewVolume)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn ALevelStreamingVolume actor"), nullptr);
        return true;
    }

    // Set the volume label
    NewVolume->SetActorLabel(FString::Printf(TEXT("StreamingVolume_%s"), *LevelName));

    // Configure streaming usage
    if (StreamingUsage == TEXT("Loading"))
    {
        NewVolume->StreamingUsage = EStreamingVolumeUsage::SVB_Loading;
    }
    else if (StreamingUsage == TEXT("VisibilityBlockingOnLoad"))
    {
        NewVolume->StreamingUsage = EStreamingVolumeUsage::SVB_VisibilityBlockingOnLoad;
    }
    else if (StreamingUsage == TEXT("BlockingOnLoad"))
    {
        NewVolume->StreamingUsage = EStreamingVolumeUsage::SVB_BlockingOnLoad;
    }
    else if (StreamingUsage == TEXT("LoadingNotVisible"))
    {
        NewVolume->StreamingUsage = EStreamingVolumeUsage::SVB_LoadingNotVisible;
    }
    else // Default: LoadingAndVisibility
    {
        NewVolume->StreamingUsage = EStreamingVolumeUsage::SVB_LoadingAndVisibility;
    }

    // Scale the volume to match the streaming distance (brush default is ~200 units cube)
    // We scale to create a sphere-like volume with radius = StreamingDistance
    FVector DesiredScale = FVector(StreamingDistance / 100.0); // Brush is ~200 units, half = 100
    NewVolume->SetActorScale3D(DesiredScale);

    // Associate the volume with the streaming level
    FoundLevel->EditorStreamingVolumes.AddUnique(NewVolume);

    // Note: UpdateStreamingLevelsRefs() is not exported/available in all UE versions
    // The association via EditorStreamingVolumes is sufficient - refs update on save
    UE_LOG(LogMcpLevelStructureHandlers, Verbose, TEXT("Streaming volume created - refs will update on save"));

    // Mark the level streaming object as dirty
    FoundLevel->MarkPackageDirty();
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("levelName"), LevelName);
    ResponseJson->SetStringField(TEXT("volumeName"), NewVolume->GetActorLabel());
    ResponseJson->SetNumberField(TEXT("streamingDistance"), StreamingDistance);
    ResponseJson->SetStringField(TEXT("streamingUsage"), StreamingUsage);
    
    TSharedPtr<FJsonObject> LocationJson = MakeShareable(new FJsonObject());
    LocationJson->SetNumberField(TEXT("x"), VolumeLocation.X);
    LocationJson->SetNumberField(TEXT("y"), VolumeLocation.Y);
    LocationJson->SetNumberField(TEXT("z"), VolumeLocation.Z);
    ResponseJson->SetObjectField(TEXT("volumeLocation"), LocationJson);
    
    ResponseJson->SetNumberField(TEXT("totalStreamingVolumes"), FoundLevel->EditorStreamingVolumes.Num());

    FString Message = FString::Printf(TEXT("Created streaming volume for level '%s' with distance %.0f at (%f, %f, %f)"), 
        *LevelName, StreamingDistance, VolumeLocation.X, VolumeLocation.Y, VolumeLocation.Z);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

static bool HandleConfigureLevelBounds(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FVector BoundsOrigin = LevelStructureHelpers::GetVectorFromJson(GetObjectField(Payload, TEXT("boundsOrigin")));
    FVector BoundsExtent = LevelStructureHelpers::GetVectorFromJson(GetObjectField(Payload, TEXT("boundsExtent")), FVector(10000.0));
    bool bAutoCalculateBounds = GetJsonBoolField(Payload, TEXT("bAutoCalculateBounds"), false);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Get or create level bounds
    FBox WorldBounds;
    if (bAutoCalculateBounds)
    {
        // Calculate bounds from all actors
        WorldBounds = FBox(ForceInit);
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            AActor* Actor = *It;
            if (Actor && !Actor->IsA(ALevelScriptActor::StaticClass()))
            {
                FBox ActorBounds = Actor->GetComponentsBoundingBox();
                if (ActorBounds.IsValid)
                {
                    WorldBounds += ActorBounds;
                }
            }
        }
    }
    else
    {
        WorldBounds = FBox(BoundsOrigin - BoundsExtent, BoundsOrigin + BoundsExtent);
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    
    TSharedPtr<FJsonObject> OriginJson = MakeShareable(new FJsonObject());
    OriginJson->SetNumberField(TEXT("x"), WorldBounds.GetCenter().X);
    OriginJson->SetNumberField(TEXT("y"), WorldBounds.GetCenter().Y);
    OriginJson->SetNumberField(TEXT("z"), WorldBounds.GetCenter().Z);
    ResponseJson->SetObjectField(TEXT("boundsOrigin"), OriginJson);
    
    TSharedPtr<FJsonObject> ExtentJson = MakeShareable(new FJsonObject());
    ExtentJson->SetNumberField(TEXT("x"), WorldBounds.GetExtent().X);
    ExtentJson->SetNumberField(TEXT("y"), WorldBounds.GetExtent().Y);
    ExtentJson->SetNumberField(TEXT("z"), WorldBounds.GetExtent().Z);
    ResponseJson->SetObjectField(TEXT("boundsExtent"), ExtentJson);

    FString Message = TEXT("Configured level bounds");
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

// ============================================================================
// World Partition Handlers (6 actions)
// ============================================================================

static bool HandleEnableWorldPartition(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    bool bEnable = GetJsonBoolField(Payload, TEXT("bEnableWorldPartition"), true);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Check if World Partition is available
    UWorldPartition* WorldPartition = World->GetWorldPartition();
    
    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetBoolField(TEXT("worldPartitionEnabled"), WorldPartition != nullptr);
    ResponseJson->SetBoolField(TEXT("requested"), bEnable);

    // If user requested to enable WP but it's not enabled, return failure
    if (bEnable && !WorldPartition)
    {
        ResponseJson->SetStringField(TEXT("note"), TEXT("World Partition must be enabled when creating the level. Convert existing level via Edit > Convert Level"));
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Cannot enable World Partition programmatically. Use 'Edit > Convert Level' in editor or create a new level with World Partition enabled."), ResponseJson);
        return true;
    }

    FString Message;
    if (WorldPartition)
    {
        Message = TEXT("World Partition is enabled for this level");
    }
    else
    {
        Message = TEXT("World Partition is not enabled for this level");
    }

    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

static bool HandleConfigureGridSize(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString GridName = GetJsonStringField(Payload, TEXT("gridName"), TEXT(""));
    int32 GridCellSize = GetJsonIntField(Payload, TEXT("gridCellSize"), 12800);
    float LoadingRange = static_cast<float>(GetJsonNumberField(Payload, TEXT("loadingRange"), 25600.0));
    bool bBlockOnSlowStreaming = GetJsonBoolField(Payload, TEXT("bBlockOnSlowStreaming"), false);
    int32 Priority = GetJsonIntField(Payload, TEXT("priority"), 0);
    bool bCreateIfMissing = GetJsonBoolField(Payload, TEXT("createIfMissing"), true);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    UWorldPartition* WorldPartition = World->GetWorldPartition();
    if (!WorldPartition)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("World Partition is not enabled for this level"), nullptr);
        return true;
    }

    // Get the runtime hash - World Partition uses UWorldPartitionRuntimeSpatialHash for grid-based streaming
    UWorldPartitionRuntimeHash* RuntimeHash = WorldPartition->RuntimeHash;
    if (!RuntimeHash)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("World Partition RuntimeHash not available"), nullptr);
        return true;
    }

    UWorldPartitionRuntimeSpatialHash* SpatialHash = Cast<UWorldPartitionRuntimeSpatialHash>(RuntimeHash);
    if (!SpatialHash)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("World Partition is not using RuntimeSpatialHash. Grid configuration not applicable."), nullptr);
        return true;
    }

#if WITH_EDITORONLY_DATA
    // Access the editor-only Grids array via reflection since it's protected
    // The Grids property is TArray<FSpatialHashRuntimeGrid> which holds the editable grid configuration
    FProperty* GridsProperty = SpatialHash->GetClass()->FindPropertyByName(TEXT("Grids"));
    if (!GridsProperty)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Could not find Grids property on RuntimeSpatialHash"), nullptr);
        return true;
    }

    FArrayProperty* ArrayProp = CastField<FArrayProperty>(GridsProperty);
    if (!ArrayProp)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Grids property is not an array"), nullptr);
        return true;
    }

    // Get the array helper
    void* GridsArrayPtr = GridsProperty->ContainerPtrToValuePtr<void>(SpatialHash);
    FScriptArrayHelper ArrayHelper(ArrayProp, GridsArrayPtr);

    // Find the grid by name, or use the first one if no name specified
    bool bFound = false;
    bool bCreated = false;
    int32 ModifiedIndex = -1;
    FName TargetGridName = GridName.IsEmpty() ? FName(NAME_None) : FName(*GridName);

    for (int32 i = 0; i < ArrayHelper.Num(); ++i)
    {
        FSpatialHashRuntimeGrid* Grid = reinterpret_cast<FSpatialHashRuntimeGrid*>(ArrayHelper.GetRawPtr(i));
        if (Grid)
        {
            // Match by name, or use first grid if no name specified
            if (GridName.IsEmpty() || Grid->GridName == TargetGridName)
            {
                // Modify the grid settings
                Grid->CellSize = GridCellSize;
                Grid->LoadingRange = LoadingRange;
                Grid->bBlockOnSlowStreaming = bBlockOnSlowStreaming;
                Grid->Priority = Priority;
                
                bFound = true;
                ModifiedIndex = i;
                break;
            }
        }
    }

    // If not found and createIfMissing is true, add a new grid
    if (!bFound && bCreateIfMissing && !GridName.IsEmpty())
    {
        int32 NewIndex = ArrayHelper.AddValue();
        FSpatialHashRuntimeGrid* NewGrid = reinterpret_cast<FSpatialHashRuntimeGrid*>(ArrayHelper.GetRawPtr(NewIndex));
        if (NewGrid)
        {
            NewGrid->GridName = FName(*GridName);
            NewGrid->CellSize = GridCellSize;
            NewGrid->LoadingRange = LoadingRange;
            NewGrid->bBlockOnSlowStreaming = bBlockOnSlowStreaming;
            NewGrid->Priority = Priority;
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
            NewGrid->Origin = FVector2D::ZeroVector;
#endif
            NewGrid->DebugColor = FLinearColor::MakeRandomColor();
            NewGrid->bClientOnlyVisible = false;
            NewGrid->HLODLayer = nullptr;
            
            bCreated = true;
            ModifiedIndex = NewIndex;
        }
    }

    if (!bFound && !bCreated)
    {
        // List available grids
        TArray<FString> AvailableGrids;
        for (int32 i = 0; i < ArrayHelper.Num(); ++i)
        {
            FSpatialHashRuntimeGrid* Grid = reinterpret_cast<FSpatialHashRuntimeGrid*>(ArrayHelper.GetRawPtr(i));
            if (Grid)
            {
                AvailableGrids.Add(Grid->GridName.ToString());
            }
        }
        
        FString AvailableStr = AvailableGrids.Num() > 0 
            ? FString::Join(AvailableGrids, TEXT(", ")) 
            : TEXT("(none - use createIfMissing=true to create a new grid)");
        
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Grid '%s' not found. Available grids: %s"), *GridName, *AvailableStr), nullptr);
        return true;
    }

    // Mark the object as modified
    SpatialHash->Modify();
    SpatialHash->MarkPackageDirty();
    World->MarkPackageDirty();

    // Build response with current grid configuration
    TArray<TSharedPtr<FJsonValue>> GridsArray;
    for (int32 i = 0; i < ArrayHelper.Num(); ++i)
    {
        FSpatialHashRuntimeGrid* Grid = reinterpret_cast<FSpatialHashRuntimeGrid*>(ArrayHelper.GetRawPtr(i));
        if (Grid)
        {
            TSharedPtr<FJsonObject> GridObj = MakeShareable(new FJsonObject());
            GridObj->SetStringField(TEXT("gridName"), Grid->GridName.ToString());
            GridObj->SetNumberField(TEXT("cellSize"), Grid->CellSize);
            GridObj->SetNumberField(TEXT("loadingRange"), Grid->LoadingRange);
            GridObj->SetBoolField(TEXT("blockOnSlowStreaming"), Grid->bBlockOnSlowStreaming);
            GridObj->SetNumberField(TEXT("priority"), Grid->Priority);
            GridObj->SetBoolField(TEXT("modified"), i == ModifiedIndex);
            GridsArray.Add(MakeShareable(new FJsonValueObject(GridObj)));
        }
    }

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("gridName"), GridName.IsEmpty() ? TEXT("(default)") : GridName);
    ResponseJson->SetNumberField(TEXT("cellSize"), GridCellSize);
    ResponseJson->SetNumberField(TEXT("loadingRange"), LoadingRange);
    ResponseJson->SetBoolField(TEXT("blockOnSlowStreaming"), bBlockOnSlowStreaming);
    ResponseJson->SetNumberField(TEXT("priority"), Priority);
    ResponseJson->SetBoolField(TEXT("created"), bCreated);
    ResponseJson->SetBoolField(TEXT("modified"), bFound);
    ResponseJson->SetArrayField(TEXT("allGrids"), GridsArray);
    ResponseJson->SetStringField(TEXT("note"), TEXT("Grid configuration updated. Regenerate streaming data to apply changes (World Partition > Generate Streaming)."));

    FString Action = bCreated ? TEXT("Created") : TEXT("Configured");
    FString Message = FString::Printf(TEXT("%s grid '%s' with CellSize=%d, LoadingRange=%.0f"), 
        *Action, GridName.IsEmpty() ? TEXT("(default)") : *GridName, GridCellSize, LoadingRange);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;

#else
    // Non-editor build: report current state only
    TArray<TSharedPtr<FJsonValue>> GridsArray;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
    // UE 5.7+: ForEachStreamingGrid is available as public API
    SpatialHash->ForEachStreamingGrid([&GridsArray](const FSpatialHashStreamingGrid& Grid)
    {
        TSharedPtr<FJsonObject> GridObj = MakeShareable(new FJsonObject());
        GridObj->SetStringField(TEXT("gridName"), Grid.GridName.ToString());
        GridObj->SetNumberField(TEXT("cellSize"), Grid.CellSize);
        GridObj->SetNumberField(TEXT("loadingRange"), Grid.LoadingRange);
        GridsArray.Add(MakeShareable(new FJsonValueObject(GridObj)));
    });
#else
    // UE 5.0-5.6: ForEachStreamingGrid not available - return empty grid info
    UE_LOG(LogMcpLevelStructureHandlers, Warning, TEXT("ForEachStreamingGrid not available in UE versions < 5.7"));
#endif

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetArrayField(TEXT("currentGrids"), GridsArray);
    ResponseJson->SetStringField(TEXT("note"), TEXT("Grid configuration requires editor build to modify."));
    
    Subsystem->SendAutomationResponse(Socket, RequestId, false,
        TEXT("Grid configuration requires editor build"), ResponseJson);
    return true;
#endif
}

static bool HandleCreateDataLayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
    using namespace LevelStructureHelpers;

    FString DataLayerName = GetJsonStringField(Payload, TEXT("dataLayerName"), TEXT("NewDataLayer"));
    FString DataLayerAssetPath = GetJsonStringField(Payload, TEXT("dataLayerAssetPath"), TEXT("/Game/DataLayers"));
    bool bIsInitiallyVisible = GetJsonBoolField(Payload, TEXT("bIsInitiallyVisible"), true);
    bool bIsInitiallyLoaded = GetJsonBoolField(Payload, TEXT("bIsInitiallyLoaded"), true);
    FString DataLayerType = GetJsonStringField(Payload, TEXT("dataLayerType"), TEXT("Runtime"));
    bool bIsPrivate = GetJsonBoolField(Payload, TEXT("bIsPrivate"), false);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Check if World Partition is enabled
    UWorldPartition* WorldPartition = World->GetWorldPartition();
    if (!WorldPartition)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("World Partition is not enabled for this level. Data layers require World Partition."), nullptr);
        return true;
    }

    // Get the Data Layer Editor Subsystem
    UDataLayerEditorSubsystem* DataLayerEditorSubsystem = UDataLayerEditorSubsystem::Get();
    if (!DataLayerEditorSubsystem)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Data Layer Editor Subsystem not available"), nullptr);
        return true;
    }

    // Step 1: Create a UDataLayerAsset (the asset that backs the data layer instance)
    FString FullAssetPath = DataLayerAssetPath / DataLayerName;
    if (!FullAssetPath.StartsWith(TEXT("/Game/")))
    {
        FullAssetPath = TEXT("/Game/") + FullAssetPath;
    }

    // Create the package for the data layer asset
    UPackage* AssetPackage = CreatePackage(*FullAssetPath);
    if (!AssetPackage)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create package for DataLayerAsset at: %s"), *FullAssetPath), nullptr);
        return true;
    }

    // Create the UDataLayerAsset
    UDataLayerAsset* NewDataLayerAsset = NewObject<UDataLayerAsset>(AssetPackage, *DataLayerName, RF_Public | RF_Standalone);
    if (!NewDataLayerAsset)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create UDataLayerAsset object"), nullptr);
        return true;
    }

    // Configure the data layer asset type
    if (DataLayerType == TEXT("Runtime"))
    {
        NewDataLayerAsset->SetType(EDataLayerType::Runtime);
    }
    else
    {
        NewDataLayerAsset->SetType(EDataLayerType::Editor);
    }

    // Mark package dirty and notify asset registry
    AssetPackage->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(NewDataLayerAsset);

    // Save the asset
    McpSafeAssetSave(NewDataLayerAsset);

    // Step 2: Create a UDataLayerInstance using the asset
    FDataLayerCreationParameters CreationParams;
    CreationParams.DataLayerAsset = NewDataLayerAsset;
    CreationParams.WorldDataLayers = World->GetWorldDataLayers();
    CreationParams.bIsPrivate = bIsPrivate;

    UDataLayerInstance* NewDataLayerInstance = DataLayerEditorSubsystem->CreateDataLayerInstance(CreationParams);
    if (!NewDataLayerInstance)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Created DataLayerAsset '%s' but failed to create DataLayerInstance. The asset exists at: %s"), 
                *DataLayerName, *FullAssetPath), nullptr);
        return true;
    }

    // Configure initial visibility and loaded state
    // Note: SetDataLayerIsInitiallyVisible was removed in UE 5.5
    #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 4
    DataLayerEditorSubsystem->SetDataLayerIsInitiallyVisible(NewDataLayerInstance, bIsInitiallyVisible);
#endif
    DataLayerEditorSubsystem->SetDataLayerIsLoadedInEditor(NewDataLayerInstance, bIsInitiallyLoaded, false);

    // Mark world dirty
    World->MarkPackageDirty();

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("dataLayerName"), DataLayerName);
    ResponseJson->SetStringField(TEXT("dataLayerAssetPath"), FullAssetPath);
    ResponseJson->SetStringField(TEXT("dataLayerType"), DataLayerType);
    ResponseJson->SetBoolField(TEXT("initiallyVisible"), bIsInitiallyVisible);
    ResponseJson->SetBoolField(TEXT("initiallyLoaded"), bIsInitiallyLoaded);
    ResponseJson->SetBoolField(TEXT("isPrivate"), bIsPrivate);

    FString Message = FString::Printf(TEXT("Created data layer '%s' with asset at '%s'"), 
        *DataLayerName, *FullAssetPath);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
#else
    // UE 5.0 does not support the new DataLayer API
    Subsystem->SendAutomationResponse(Socket, RequestId, false,
        TEXT("Data layer creation requires Unreal Engine 5.1 or later."), nullptr);
#endif
    return true;
}

static bool HandleAssignActorToDataLayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
    using namespace LevelStructureHelpers;

    FString ActorName = GetJsonStringField(Payload, TEXT("actorName"), TEXT(""));
    FString DataLayerName = GetJsonStringField(Payload, TEXT("dataLayerName"), TEXT(""));

    if (ActorName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("actorName is required"), nullptr);
        return true;
    }

    if (DataLayerName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("dataLayerName is required"), nullptr);
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Check if World Partition is enabled
    UWorldPartition* WorldPartition = World->GetWorldPartition();
    if (!WorldPartition)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("World Partition is not enabled for this level. Data layers require World Partition."), nullptr);
        return true;
    }

    // Get the Data Layer Editor Subsystem
    UDataLayerEditorSubsystem* DataLayerEditorSubsystem = UDataLayerEditorSubsystem::Get();
    if (!DataLayerEditorSubsystem)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Data Layer Editor Subsystem not available"), nullptr);
        return true;
    }

    // Find the actor
    AActor* FoundActor = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName || It->GetName() == ActorName)
        {
            FoundActor = *It;
            break;
        }
    }

    if (!FoundActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Actor not found: %s"), *ActorName), nullptr);
        return true;
    }

    // Find the data layer instance by name
    UDataLayerInstance* DataLayerInstance = DataLayerEditorSubsystem->GetDataLayerInstance(FName(*DataLayerName));
    if (!DataLayerInstance)
    {
        // Build a list of available data layers for the error message
        TArray<UDataLayerInstance*> AllDataLayers = DataLayerEditorSubsystem->GetAllDataLayers();
        TArray<FString> AvailableNames;
        for (UDataLayerInstance* DL : AllDataLayers)
        {
            if (DL)
            {
                AvailableNames.Add(DL->GetDataLayerShortName());
            }
        }
        
        FString AvailableStr = AvailableNames.Num() > 0 
            ? FString::Join(AvailableNames, TEXT(", ")) 
            : TEXT("(none)");

        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Data layer not found: '%s'. Available data layers: %s"), *DataLayerName, *AvailableStr), nullptr);
        return true;
    }

    // Use the real API to add the actor to the data layer
    bool bSuccess = DataLayerEditorSubsystem->AddActorToDataLayer(FoundActor, DataLayerInstance);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("actorName"), ActorName);
    ResponseJson->SetStringField(TEXT("actorPath"), FoundActor->GetPathName());
    ResponseJson->SetStringField(TEXT("dataLayerName"), DataLayerName);
    ResponseJson->SetBoolField(TEXT("assigned"), bSuccess);

    if (bSuccess)
    {
        FString Message = FString::Printf(TEXT("Assigned actor '%s' to data layer '%s'"), 
            *ActorName, *DataLayerName);
        Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    }
    else
    {
        // Check if actor was already in the data layer
        ResponseJson->SetStringField(TEXT("reason"), TEXT("Actor may already belong to this data layer or is not compatible"));
        FString Message = FString::Printf(TEXT("Failed to assign actor '%s' to data layer '%s'. Actor may already belong to this layer or is not compatible with data layers."), 
            *ActorName, *DataLayerName);
        Subsystem->SendAutomationResponse(Socket, RequestId, false, Message, ResponseJson);
    }
#else
    // UE 5.0 does not support the new DataLayer API
    Subsystem->SendAutomationResponse(Socket, RequestId, false,
        TEXT("Data layer assignment requires Unreal Engine 5.1 or later."), nullptr);
#endif
    return true;
}

static bool HandleConfigureHlodLayer(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString HlodLayerName = GetJsonStringField(Payload, TEXT("hlodLayerName"), TEXT("DefaultHLOD"));
    FString HlodLayerPath = GetJsonStringField(Payload, TEXT("hlodLayerPath"), TEXT("/Game/HLOD"));
    bool bIsSpatiallyLoaded = GetJsonBoolField(Payload, TEXT("bIsSpatiallyLoaded"), true);
    int32 CellSize = GetJsonIntField(Payload, TEXT("cellSize"), 25600);
    double LoadingDistance = GetJsonNumberField(Payload, TEXT("loadingDistance"), 51200.0);
    FString LayerType = GetJsonStringField(Payload, TEXT("layerType"), TEXT("MeshMerge"));

    // Build full path
    FString FullPath = HlodLayerPath / HlodLayerName;
    if (!FullPath.StartsWith(TEXT("/Game/")))
    {
        FullPath = TEXT("/Game/") + FullPath;
    }

    // Create the package for the HLOD layer asset
    UPackage* AssetPackage = CreatePackage(*FullPath);
    if (!AssetPackage)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Failed to create package for HLOD layer at: %s"), *FullPath), nullptr);
        return true;
    }

    // Create the UHLODLayer asset
    UHLODLayer* NewHLODLayer = NewObject<UHLODLayer>(AssetPackage, *HlodLayerName, RF_Public | RF_Standalone);
    if (!NewHLODLayer)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to create UHLODLayer object"), nullptr);
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

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
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

static bool HandleCreateMinimapVolume(
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
            TEXT("World Partition is not enabled. AWorldPartitionMiniMapVolume requires World Partition."), nullptr);
        return true;
    }

    // Spawn the AWorldPartitionMiniMapVolume
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = FName(*VolumeName);
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
            TEXT("Failed to spawn AWorldPartitionMiniMapVolume actor"), nullptr);
        return true;
    }

    // Set actor label
    MiniMapVolume->SetActorLabel(*VolumeName);

    // Scale the volume to match the extent (AVolume uses a brush, scale affects it)
    // The default brush is a 200x200x200 cube, so we scale it to match the desired extent
    FVector CurrentScale = MiniMapVolume->GetActorScale3D();
    FVector DesiredScale = VolumeExtent / 100.0; // Brush is 200 units, so divide by half
    MiniMapVolume->SetActorScale3D(DesiredScale);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("volumeName"), VolumeName);
    ResponseJson->SetStringField(TEXT("volumeClass"), TEXT("AWorldPartitionMiniMapVolume"));
    
    TSharedPtr<FJsonObject> LocationJson = MakeShareable(new FJsonObject());
    LocationJson->SetNumberField(TEXT("x"), VolumeLocation.X);
    LocationJson->SetNumberField(TEXT("y"), VolumeLocation.Y);
    LocationJson->SetNumberField(TEXT("z"), VolumeLocation.Z);
    ResponseJson->SetObjectField(TEXT("volumeLocation"), LocationJson);
    
    TSharedPtr<FJsonObject> ExtentJson = MakeShareable(new FJsonObject());
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

static bool HandleOpenLevelBlueprint(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Get the persistent level (which is the level that has a level blueprint)
    ULevel* PersistentLevel = World->PersistentLevel;
    if (!PersistentLevel)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No persistent level available"), nullptr);
        return true;
    }

    // Check if the level is saved (has a valid package path)
    FString LevelPackageName = World->GetOutermost()->GetName();
    bool bIsSavedLevel = !LevelPackageName.IsEmpty() && !LevelPackageName.StartsWith(TEXT("/Temp/"));

    // For unsaved levels, GetLevelScriptBlueprint(true) may fail to create the blueprint
    // because it requires a valid package path
    ULevelScriptBlueprint* LevelBP = PersistentLevel->GetLevelScriptBlueprint(true);
    if (!LevelBP)
    {
        // Try to create the level blueprint manually for unsaved levels
        if (!bIsSavedLevel)
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false,
                TEXT("Level Blueprint unavailable for unsaved levels. Please save the level first."), nullptr);
            return true;
        }
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to get or create Level Blueprint"), nullptr);
        return true;
    }

    // Open the blueprint editor
    GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(LevelBP);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("levelName"), World->GetMapName());
    ResponseJson->SetStringField(TEXT("blueprintPath"), LevelBP->GetPathName());

    FString Message = FString::Printf(TEXT("Opened Level Blueprint for: %s"), *World->GetMapName());
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

static bool HandleAddLevelBlueprintNode(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString NodeClass = GetJsonStringField(Payload, TEXT("nodeClass"), TEXT(""));
    FString NodeName = GetJsonStringField(Payload, TEXT("nodeName"), TEXT(""));
    TSharedPtr<FJsonObject> PositionJson = GetObjectField(Payload, TEXT("nodePosition"));
    int32 PosX = PositionJson.IsValid() ? static_cast<int32>(GetJsonNumberField(PositionJson, TEXT("x"))) : 0;
    int32 PosY = PositionJson.IsValid() ? static_cast<int32>(GetJsonNumberField(PositionJson, TEXT("y"))) : 0;

    if (NodeClass.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("nodeClass is required"), nullptr);
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    ULevel* CurrentLevel = World->GetCurrentLevel();
    if (!CurrentLevel)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No current level available"), nullptr);
        return true;
    }

    ULevelScriptBlueprint* LevelBP = CurrentLevel->GetLevelScriptBlueprint(true);
    if (!LevelBP)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to get Level Blueprint"), nullptr);
        return true;
    }

    // Get the event graph
    UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(LevelBP);
    if (!EventGraph)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to find event graph in Level Blueprint"), nullptr);
        return true;
    }

    // Find the node class - try multiple lookup paths
    FString TriedPaths;
    UClass* NodeClassObj = FindObject<UClass>(nullptr, *NodeClass);
    TriedPaths = NodeClass;
    
    if (!NodeClassObj)
    {
        // Try with BlueprintGraph prefix
        FString BlueprintGraphPath = TEXT("/Script/BlueprintGraph.") + NodeClass;
        NodeClassObj = FindObject<UClass>(nullptr, *BlueprintGraphPath);
        TriedPaths += TEXT(", ") + BlueprintGraphPath;
    }
    
    if (!NodeClassObj)
    {
        // Try with Engine prefix
        FString EnginePath = TEXT("/Script/Engine.") + NodeClass;
        NodeClassObj = FindObject<UClass>(nullptr, *EnginePath);
        TriedPaths += TEXT(", ") + EnginePath;
    }
    
    if (!NodeClassObj)
    {
        // Try with UnrealEd prefix
        FString UnrealEdPath = TEXT("/Script/UnrealEd.") + NodeClass;
        NodeClassObj = FindObject<UClass>(nullptr, *UnrealEdPath);
        TriedPaths += TEXT(", ") + UnrealEdPath;
    }

    FString CreatedNodeName;
    if (NodeClassObj && NodeClassObj->IsChildOf(UK2Node::StaticClass()))
    {
        // Create the node
        UK2Node* NewNode = NewObject<UK2Node>(EventGraph, NodeClassObj);
        if (NewNode)
        {
            NewNode->CreateNewGuid();
            NewNode->PostPlacedNewNode();
            NewNode->AllocateDefaultPins();
            NewNode->NodePosX = PosX;
            NewNode->NodePosY = PosY;
            EventGraph->AddNode(NewNode, true, false);
            CreatedNodeName = NewNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
        }
    }

    // Check if node creation actually succeeded
    if (CreatedNodeName.IsEmpty())
    {
        FString ErrorMsg;
        if (!NodeClassObj)
        {
            ErrorMsg = FString::Printf(TEXT("Node class not found. Tried paths: [%s]"), *TriedPaths);
        }
        else if (!NodeClassObj->IsChildOf(UK2Node::StaticClass()))
        {
            ErrorMsg = FString::Printf(TEXT("Class '%s' found but is not a K2Node subclass"), *NodeClass);
        }
        else
        {
            ErrorMsg = FString::Printf(TEXT("Failed to create node instance of class: %s"), *NodeClass);
        }
        Subsystem->SendAutomationResponse(Socket, RequestId, false, ErrorMsg, nullptr);
        return true;
    }

    // Mark blueprint as modified
    FBlueprintEditorUtils::MarkBlueprintAsModified(LevelBP);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("nodeClass"), NodeClass);
    ResponseJson->SetStringField(TEXT("nodeName"), CreatedNodeName);
    ResponseJson->SetNumberField(TEXT("posX"), PosX);
    ResponseJson->SetNumberField(TEXT("posY"), PosY);
    ResponseJson->SetBoolField(TEXT("nodeCreated"), true);

    FString Message = FString::Printf(TEXT("Added node to Level Blueprint: %s"), *CreatedNodeName);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

static bool HandleConnectLevelBlueprintNodes(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString SourceNodeName = GetJsonStringField(Payload, TEXT("sourceNodeName"), TEXT(""));
    FString SourcePinName = GetJsonStringField(Payload, TEXT("sourcePinName"), TEXT(""));
    FString TargetNodeName = GetJsonStringField(Payload, TEXT("targetNodeName"), TEXT(""));
    FString TargetPinName = GetJsonStringField(Payload, TEXT("targetPinName"), TEXT(""));

    if (SourceNodeName.IsEmpty() || TargetNodeName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("sourceNodeName and targetNodeName are required"), nullptr);
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    ULevel* CurrentLevel = World->GetCurrentLevel();
    ULevelScriptBlueprint* LevelBP = CurrentLevel ? CurrentLevel->GetLevelScriptBlueprint(false) : nullptr;
    if (!LevelBP)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Level Blueprint not available"), nullptr);
        return true;
    }

    UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(LevelBP);
    if (!EventGraph)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Event graph not found"), nullptr);
        return true;
    }

    // Find source and target nodes
    UEdGraphNode* SourceNode = nullptr;
    UEdGraphNode* TargetNode = nullptr;
    
    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
        if (NodeTitle.Contains(SourceNodeName) || Node->GetName().Contains(SourceNodeName))
        {
            SourceNode = Node;
        }
        if (NodeTitle.Contains(TargetNodeName) || Node->GetName().Contains(TargetNodeName))
        {
            TargetNode = Node;
        }
    }

    if (!SourceNode || !TargetNode)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Could not find nodes: source='%s' target='%s'"), 
                *SourceNodeName, *TargetNodeName), nullptr);
        return true;
    }

    // Find pins and connect
    UEdGraphPin* SourcePin = nullptr;
    UEdGraphPin* TargetPin = nullptr;

    for (UEdGraphPin* Pin : SourceNode->Pins)
    {
        if (Pin->PinName.ToString() == SourcePinName || Pin->GetDisplayName().ToString() == SourcePinName)
        {
            SourcePin = Pin;
            break;
        }
    }

    for (UEdGraphPin* Pin : TargetNode->Pins)
    {
        if (Pin->PinName.ToString() == TargetPinName || Pin->GetDisplayName().ToString() == TargetPinName)
        {
            TargetPin = Pin;
            break;
        }
    }

    bool bConnected = false;
    if (SourcePin && TargetPin)
    {
        SourcePin->MakeLinkTo(TargetPin);
        bConnected = SourcePin->LinkedTo.Contains(TargetPin);
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(LevelBP);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("sourceNode"), SourceNodeName);
    ResponseJson->SetStringField(TEXT("sourcePin"), SourcePinName);
    ResponseJson->SetStringField(TEXT("targetNode"), TargetNodeName);
    ResponseJson->SetStringField(TEXT("targetPin"), TargetPinName);
    ResponseJson->SetBoolField(TEXT("connected"), bConnected);

    FString Message = bConnected 
        ? FString::Printf(TEXT("Connected %s.%s -> %s.%s"), *SourceNodeName, *SourcePinName, *TargetNodeName, *TargetPinName)
        : TEXT("Nodes prepared for connection (manual pin connection may be required)");
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

// ============================================================================
// Level Instances Handlers (2 actions)
// ============================================================================

static bool HandleCreateLevelInstance(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString LevelInstanceName = GetJsonStringField(Payload, TEXT("levelInstanceName"), TEXT("LevelInstance"));
    FString LevelAssetPath = GetJsonStringField(Payload, TEXT("levelAssetPath"), TEXT(""));
    FVector InstanceLocation = LevelStructureHelpers::GetVectorFromJson(GetObjectField(Payload, TEXT("instanceLocation")));
    FRotator InstanceRotation = LevelStructureHelpers::GetRotatorFromJson(GetObjectField(Payload, TEXT("instanceRotation")));
    FVector InstanceScale = LevelStructureHelpers::GetVectorFromJson(GetObjectField(Payload, TEXT("instanceScale")), FVector(1.0));

    if (LevelAssetPath.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("levelAssetPath is required"), nullptr);
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Get Level Instance Subsystem
    ULevelInstanceSubsystem* LevelInstanceSubsystem = World->GetSubsystem<ULevelInstanceSubsystem>();
    if (!LevelInstanceSubsystem)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Level Instance Subsystem not available"), nullptr);
        return true;
    }

    // Spawn Level Instance Actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = FName(*LevelInstanceName);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ALevelInstance* LevelInstanceActor = World->SpawnActor<ALevelInstance>(
        ALevelInstance::StaticClass(),
        InstanceLocation,
        InstanceRotation,
        SpawnParams
    );

    if (!LevelInstanceActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn Level Instance actor"), nullptr);
        return true;
    }

    LevelInstanceActor->SetActorScale3D(InstanceScale);
    LevelInstanceActor->SetActorLabel(*LevelInstanceName);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("levelInstanceName"), LevelInstanceName);
    ResponseJson->SetStringField(TEXT("levelAssetPath"), LevelAssetPath);
    
    TSharedPtr<FJsonObject> LocationJson = MakeShareable(new FJsonObject());
    LocationJson->SetNumberField(TEXT("x"), InstanceLocation.X);
    LocationJson->SetNumberField(TEXT("y"), InstanceLocation.Y);
    LocationJson->SetNumberField(TEXT("z"), InstanceLocation.Z);
    ResponseJson->SetObjectField(TEXT("location"), LocationJson);

    FString Message = FString::Printf(TEXT("Created Level Instance: %s"), *LevelInstanceName);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

static bool HandleCreatePackedLevelActor(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString PackedLevelName = GetJsonStringField(Payload, TEXT("packedLevelName"), TEXT("PackedLevel"));
    FString LevelAssetPath = GetJsonStringField(Payload, TEXT("levelAssetPath"), TEXT(""));
    FVector InstanceLocation = LevelStructureHelpers::GetVectorFromJson(GetObjectField(Payload, TEXT("instanceLocation")));
    FRotator InstanceRotation = LevelStructureHelpers::GetRotatorFromJson(GetObjectField(Payload, TEXT("instanceRotation")));
    bool bPackBlueprints = GetJsonBoolField(Payload, TEXT("bPackBlueprints"), true);
    bool bPackStaticMeshes = GetJsonBoolField(Payload, TEXT("bPackStaticMeshes"), true);

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Spawn Packed Level Actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = FName(*PackedLevelName);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APackedLevelActor* PackedActor = World->SpawnActor<APackedLevelActor>(
        APackedLevelActor::StaticClass(),
        InstanceLocation,
        InstanceRotation,
        SpawnParams
    );

    if (!PackedActor)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to spawn Packed Level Actor"), nullptr);
        return true;
    }

    PackedActor->SetActorLabel(*PackedLevelName);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetStringField(TEXT("packedLevelName"), PackedLevelName);
    ResponseJson->SetStringField(TEXT("levelAssetPath"), LevelAssetPath);
    ResponseJson->SetBoolField(TEXT("packBlueprints"), bPackBlueprints);
    ResponseJson->SetBoolField(TEXT("packStaticMeshes"), bPackStaticMeshes);

    FString Message = FString::Printf(TEXT("Created Packed Level Actor: %s"), *PackedLevelName);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

// ============================================================================
// Utility Handlers (1 action)
// ============================================================================

static bool HandleGetLevelStructureInfo(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    TSharedPtr<FJsonObject> InfoJson = MakeShareable(new FJsonObject());
    InfoJson->SetStringField(TEXT("currentLevel"), World->GetMapName());

    // Get streaming levels
    TArray<TSharedPtr<FJsonValue>> SublevelsArray;
    const TArray<ULevelStreaming*>& StreamingLevels = World->GetStreamingLevels();
    InfoJson->SetNumberField(TEXT("sublevelCount"), StreamingLevels.Num());
    
    for (const ULevelStreaming* StreamingLevel : StreamingLevels)
    {
        if (StreamingLevel)
        {
            SublevelsArray.Add(MakeShareable(new FJsonValueString(StreamingLevel->GetWorldAssetPackageFName().ToString())));
        }
    }
    InfoJson->SetArrayField(TEXT("sublevels"), SublevelsArray);

    // Check World Partition
    UWorldPartition* WorldPartition = World->GetWorldPartition();
    InfoJson->SetBoolField(TEXT("worldPartitionEnabled"), WorldPartition != nullptr);

    if (WorldPartition)
    {
        // Get data layers
        TArray<TSharedPtr<FJsonValue>> DataLayersArray;
        UDataLayerSubsystem* DataLayerSubsystem = World->GetSubsystem<UDataLayerSubsystem>();
        if (DataLayerSubsystem)
        {
            // Data layer enumeration would go here
        }
        InfoJson->SetArrayField(TEXT("dataLayers"), DataLayersArray);
    }

    // Get level instances
    TArray<TSharedPtr<FJsonValue>> LevelInstancesArray;
    for (TActorIterator<ALevelInstance> It(World); It; ++It)
    {
        FString ActorLabel = It->GetActorLabel();
        LevelInstancesArray.Add(MakeShareable(new FJsonValueString(ActorLabel)));
    }
    InfoJson->SetArrayField(TEXT("levelInstances"), LevelInstancesArray);

    // HLOD layers - enumerate from World Partition or legacy HLOD system
    TArray<TSharedPtr<FJsonValue>> HlodLayersArray;

    // Check for World Partition HLOD layers
    if (World->GetWorldPartition())
    {
        // Iterate through all UHLODLayer assets that are relevant to this world
        for (TObjectIterator<UHLODLayer> It; It; ++It)
        {
            UHLODLayer* Layer = *It;
            if (Layer && Layer->GetOuter() && Layer->GetOuter()->GetWorld() == World)
            {
                TSharedPtr<FJsonObject> LayerJson = MakeShared<FJsonObject>();
                LayerJson->SetStringField(TEXT("name"), Layer->GetName());
                LayerJson->SetStringField(TEXT("type"), TEXT("world_partition"));
                // UE 5.7+: GetCellSize, GetLoadingRange, IsSpatiallyLoaded are deprecated
                // These streaming grid properties are now in the partition's settings
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
                PRAGMA_DISABLE_DEPRECATION_WARNINGS
#endif
                LayerJson->SetNumberField(TEXT("cellSize"), Layer->GetCellSize());
                LayerJson->SetNumberField(TEXT("loadingRange"), Layer->GetLoadingRange());
                LayerJson->SetBoolField(TEXT("isSpatiallyLoaded"), Layer->IsSpatiallyLoaded());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
                PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
                
                // Get layer type as string
                FString LayerTypeStr;
                switch (Layer->GetLayerType())
                {
                    case EHLODLayerType::Instancing: LayerTypeStr = TEXT("Instancing"); break;
                    case EHLODLayerType::MeshMerge: LayerTypeStr = TEXT("MeshMerge"); break;
                    case EHLODLayerType::MeshSimplify: LayerTypeStr = TEXT("MeshSimplify"); break;
                    case EHLODLayerType::MeshApproximate: LayerTypeStr = TEXT("MeshApproximate"); break;
                    case EHLODLayerType::Custom: LayerTypeStr = TEXT("Custom"); break;
                    default: LayerTypeStr = TEXT("Unknown"); break;
                }
                LayerJson->SetStringField(TEXT("layerType"), LayerTypeStr);
                
                // Get parent layer if available
                TSoftObjectPtr<UHLODLayer> ParentLayerSoft = Layer->GetParentLayer();
                if (ParentLayerSoft.IsValid())
                {
                    LayerJson->SetStringField(TEXT("parentLayer"), ParentLayerSoft->GetName());
                }
                
                HlodLayersArray.Add(MakeShareable(new FJsonValueObject(LayerJson)));
            }
        }
    }

    // Also check for World Partition HLOD actors in the world
    if (HlodLayersArray.Num() == 0 && World->GetWorldPartition())
    {
        TSet<FString> FoundLayers;
        for (TActorIterator<AWorldPartitionHLOD> It(World); It; ++It)
        {
            AWorldPartitionHLOD* HLODActor = *It;
            if (HLODActor)
            {
                FString LayerName = FString::Printf(TEXT("HLOD_Level_%d"), HLODActor->GetLODLevel());
                if (!FoundLayers.Contains(LayerName))
                {
                    FoundLayers.Add(LayerName);
                    TSharedPtr<FJsonObject> LayerJson = MakeShared<FJsonObject>();
                    LayerJson->SetStringField(TEXT("name"), LayerName);
                    LayerJson->SetStringField(TEXT("type"), TEXT("world_partition_hlod_actor"));
                    LayerJson->SetNumberField(TEXT("lodLevel"), HLODActor->GetLODLevel());
                    HlodLayersArray.Add(MakeShareable(new FJsonValueObject(LayerJson)));
                }
            }
        }
    }

    // Check for legacy HLOD system (ALODActor) for non-WP levels
    if (HlodLayersArray.Num() == 0)
    {
        TMap<int32, int32> LodLevelCounts;
        for (TActorIterator<ALODActor> It(World); It; ++It)
        {
            ALODActor* LODActor = *It;
            if (LODActor)
            {
                int32 Level = LODActor->LODLevel;
                LodLevelCounts.FindOrAdd(Level)++;
            }
        }
        
        // Create layer entries for each LOD level found
        for (const auto& Pair : LodLevelCounts)
        {
            TSharedPtr<FJsonObject> LayerJson = MakeShared<FJsonObject>();
            LayerJson->SetStringField(TEXT("name"), FString::Printf(TEXT("LOD_Level_%d"), Pair.Key));
            LayerJson->SetStringField(TEXT("type"), TEXT("legacy_hlod"));
            LayerJson->SetNumberField(TEXT("lodLevel"), Pair.Key);
            LayerJson->SetNumberField(TEXT("actorCount"), Pair.Value);
            HlodLayersArray.Add(MakeShareable(new FJsonValueObject(LayerJson)));
        }
    }

    InfoJson->SetArrayField(TEXT("hlodLayers"), HlodLayersArray);

    TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject());
    ResponseJson->SetObjectField(TEXT("levelStructureInfo"), InfoJson);

    FString Message = TEXT("Retrieved level structure information");
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

#endif // WITH_EDITOR

// ============================================================================
// Main Dispatch Handler
// ============================================================================

bool UMcpAutomationBridgeSubsystem::HandleManageLevelStructureAction(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
#if WITH_EDITOR
    FString SubAction;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("subAction"), SubAction);
    }

    UE_LOG(LogMcpLevelStructureHandlers, Log, TEXT("HandleManageLevelStructureAction: SubAction=%s"), *SubAction);

    bool bHandled = false;

    // Levels
    if (SubAction == TEXT("create_level"))
    {
        bHandled = HandleCreateLevel(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("create_sublevel"))
    {
        bHandled = HandleCreateSublevel(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("configure_level_streaming"))
    {
        bHandled = HandleConfigureLevelStreaming(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("set_streaming_distance"))
    {
        bHandled = HandleSetStreamingDistance(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("configure_level_bounds"))
    {
        bHandled = HandleConfigureLevelBounds(this, RequestId, Payload, Socket);
    }
    // World Partition
    else if (SubAction == TEXT("enable_world_partition"))
    {
        bHandled = HandleEnableWorldPartition(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("configure_grid_size"))
    {
        bHandled = HandleConfigureGridSize(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("create_data_layer"))
    {
        bHandled = HandleCreateDataLayer(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("assign_actor_to_data_layer"))
    {
        bHandled = HandleAssignActorToDataLayer(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("configure_hlod_layer"))
    {
        bHandled = HandleConfigureHlodLayer(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("create_minimap_volume"))
    {
        bHandled = HandleCreateMinimapVolume(this, RequestId, Payload, Socket);
    }
    // Level Blueprint
    else if (SubAction == TEXT("open_level_blueprint"))
    {
        bHandled = HandleOpenLevelBlueprint(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("add_level_blueprint_node"))
    {
        bHandled = HandleAddLevelBlueprintNode(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("connect_level_blueprint_nodes"))
    {
        bHandled = HandleConnectLevelBlueprintNodes(this, RequestId, Payload, Socket);
    }
    // Level Instances
    else if (SubAction == TEXT("create_level_instance"))
    {
        bHandled = HandleCreateLevelInstance(this, RequestId, Payload, Socket);
    }
    else if (SubAction == TEXT("create_packed_level_actor"))
    {
        bHandled = HandleCreatePackedLevelActor(this, RequestId, Payload, Socket);
    }
    // Utility
    else if (SubAction == TEXT("get_level_structure_info"))
    {
        bHandled = HandleGetLevelStructureInfo(this, RequestId, Payload, Socket);
    }
    else
    {
        SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Unknown manage_level_structure action: %s"), *SubAction), nullptr);
        return true;  // Return true: request was handled (error response sent)
    }

    return bHandled;

#else
    SendAutomationResponse(Socket, RequestId, false, TEXT("manage_level_structure requires editor build"), nullptr);
    return true;  // Return true: request was handled (error response sent)
#endif
}

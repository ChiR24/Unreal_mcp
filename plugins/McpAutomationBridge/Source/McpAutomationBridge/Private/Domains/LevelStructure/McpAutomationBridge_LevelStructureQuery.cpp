#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureActions.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"

#include "Engine/LODActor.h"
#include "Engine/LevelStreaming.h"
#include "Engine/LevelStreamingAlwaysLoaded.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "WorldPartition/DataLayer/DataLayerSubsystem.h"
#if __has_include("WorldPartition/DataLayer/DataLayerManager.h")
#include "WorldPartition/DataLayer/DataLayerManager.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"
#define MCP_HAS_DATA_LAYER_MANAGER 1
#else
#define MCP_HAS_DATA_LAYER_MANAGER 0
#endif
#include "Engine/Level.h"
#include "Misc/PackageName.h"
#include "WorldPartition/HLOD/HLODActor.h"
#include "WorldPartition/HLOD/HLODLayer.h"
#include "WorldPartition/WorldPartition.h"

#if WITH_EDITOR
namespace McpLevelStructure
{

bool HandleGetLevelStructureInfo(
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

    TSharedPtr<FJsonObject> InfoJson = McpHandlerUtils::CreateResultObject();
    InfoJson->SetStringField(TEXT("currentLevel"), World->GetMapName());

    TArray<TSharedPtr<FJsonValue>> SublevelsArray;
    const TArray<ULevelStreaming*>& StreamingLevels = World->GetStreamingLevels();
    InfoJson->SetNumberField(TEXT("sublevelCount"), StreamingLevels.Num());

    for (const ULevelStreaming* StreamingLevel : StreamingLevels)
    {
        if (StreamingLevel)
        {
            SublevelsArray.Add(MakeShared<FJsonValueString>(StreamingLevel->GetWorldAssetPackageFName().ToString()));
        }
    }
    InfoJson->SetArrayField(TEXT("sublevels"), SublevelsArray);
    // Structured streaming-level report (dogfood #161): the bare name list said nothing about state.
    TArray<TSharedPtr<FJsonValue>> StreamingArray;
    for (const ULevelStreaming* StreamingLevel : StreamingLevels)
    {
        if (!StreamingLevel) { continue; }
        TSharedPtr<FJsonObject> LevelJson = McpHandlerUtils::CreateResultObject();
        const FString PackageName = StreamingLevel->GetWorldAssetPackageName();
        LevelJson->SetStringField(TEXT("name"), FPackageName::GetShortName(PackageName));
        LevelJson->SetStringField(TEXT("packageName"), PackageName);
        LevelJson->SetBoolField(TEXT("isLoaded"), StreamingLevel->IsLevelLoaded());
        LevelJson->SetBoolField(TEXT("isVisible"), StreamingLevel->IsLevelVisible());
        LevelJson->SetBoolField(TEXT("shouldBeLoaded"), StreamingLevel->ShouldBeLoaded());
        LevelJson->SetBoolField(TEXT("shouldBeVisible"), StreamingLevel->ShouldBeVisible());
        LevelJson->SetStringField(TEXT("streamingClass"), StreamingLevel->GetClass()->GetName());
        LevelJson->SetBoolField(TEXT("alwaysLoaded"), StreamingLevel->IsA(ULevelStreamingAlwaysLoaded::StaticClass()));
        LevelJson->SetNumberField(TEXT("actorCount"), StreamingLevel->GetLoadedLevel() ? StreamingLevel->GetLoadedLevel()->Actors.Num() : 0);
        StreamingArray.Add(MakeShared<FJsonValueObject>(LevelJson));
    }
    InfoJson->SetArrayField(TEXT("streamingLevels"), StreamingArray);
    InfoJson->SetStringField(TEXT("persistentLevel"), World->GetOutermost()->GetName());
    InfoJson->SetNumberField(TEXT("actorCount"), World->PersistentLevel ? World->PersistentLevel->Actors.Num() : 0);

    UWorldPartition* WorldPartition = World->GetWorldPartition();
    InfoJson->SetBoolField(TEXT("worldPartitionEnabled"), WorldPartition != nullptr);

    if (WorldPartition)
    {
        TArray<TSharedPtr<FJsonValue>> DataLayersArray;
#if MCP_HAS_DATA_LAYER_MANAGER
        if (UDataLayerManager* DataLayerManager = UDataLayerManager::GetDataLayerManager(World))
        {
            DataLayerManager->ForEachDataLayerInstance([&DataLayersArray](UDataLayerInstance* Instance)
            {
                if (!Instance) { return true; }
                TSharedPtr<FJsonObject> LayerJson = McpHandlerUtils::CreateResultObject();
                LayerJson->SetStringField(TEXT("name"), Instance->GetDataLayerShortName());
                LayerJson->SetStringField(TEXT("fullName"), Instance->GetDataLayerFullName());
                LayerJson->SetBoolField(TEXT("isRuntime"), Instance->IsRuntime());
                LayerJson->SetStringField(TEXT("runtimeState"), StaticEnum<EDataLayerRuntimeState>()->GetNameStringByValue(static_cast<int64>(Instance->GetRuntimeState())));
                LayerJson->SetStringField(TEXT("initialRuntimeState"), StaticEnum<EDataLayerRuntimeState>()->GetNameStringByValue(static_cast<int64>(Instance->GetInitialRuntimeState())));
                DataLayersArray.Add(MakeShared<FJsonValueObject>(LayerJson));
                return true;
            });
        }
#endif
        InfoJson->SetArrayField(TEXT("dataLayers"), DataLayersArray);
    }

    TArray<TSharedPtr<FJsonValue>> LevelInstancesArray;
    for (TActorIterator<ALevelInstance> It(World); It; ++It)
    {
        FString ActorLabel = It->GetActorLabel();
        LevelInstancesArray.Add(MakeShared<FJsonValueString>(ActorLabel));
    }
    InfoJson->SetArrayField(TEXT("levelInstances"), LevelInstancesArray);

    // HLOD layers - enumerate from World Partition or legacy HLOD system
    TArray<TSharedPtr<FJsonValue>> HlodLayersArray;

    if (World->GetWorldPartition())
    {
        for (TObjectIterator<UHLODLayer> It; It; ++It)
        {
            UHLODLayer* Layer = *It;
            if (Layer && Layer->GetOuter() && Layer->GetOuter()->GetWorld() == World)
            {
                TSharedPtr<FJsonObject> LayerJson = McpHandlerUtils::CreateResultObject();
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

                TSoftObjectPtr<UHLODLayer> ParentLayerSoft = Layer->GetParentLayer();
                if (ParentLayerSoft.IsValid())
                {
                    LayerJson->SetStringField(TEXT("parentLayer"), ParentLayerSoft->GetName());
                }

                HlodLayersArray.Add(MakeShared<FJsonValueObject>(LayerJson));
            }
        }
    }

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
                    TSharedPtr<FJsonObject> LayerJson = McpHandlerUtils::CreateResultObject();
                    LayerJson->SetStringField(TEXT("name"), LayerName);
                    LayerJson->SetStringField(TEXT("type"), TEXT("world_partition_hlod_actor"));
                    LayerJson->SetNumberField(TEXT("lodLevel"), HLODActor->GetLODLevel());
                    HlodLayersArray.Add(MakeShared<FJsonValueObject>(LayerJson));
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
            TSharedPtr<FJsonObject> LayerJson = McpHandlerUtils::CreateResultObject();
            LayerJson->SetStringField(TEXT("name"), FString::Printf(TEXT("LOD_Level_%d"), Pair.Key));
            LayerJson->SetStringField(TEXT("type"), TEXT("legacy_hlod"));
            LayerJson->SetNumberField(TEXT("lodLevel"), Pair.Key);
            LayerJson->SetNumberField(TEXT("actorCount"), Pair.Value);
            HlodLayersArray.Add(MakeShared<FJsonValueObject>(LayerJson));
        }
    }

    InfoJson->SetArrayField(TEXT("hlodLayers"), HlodLayersArray);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    ResponseJson->SetObjectField(TEXT("levelStructureInfo"), InfoJson);

    FString Message = TEXT("Retrieved level structure information");
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

}
#endif

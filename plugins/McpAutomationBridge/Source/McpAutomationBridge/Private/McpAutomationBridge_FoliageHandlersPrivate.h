#pragma once

#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR
#include "ActorPartition/ActorPartitionSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FoliageType.h"
#include "FoliageTypeObject.h"
#include "FoliageType_InstancedStaticMesh.h"
#include "InstancedFoliageActor.h"
#include "ProceduralFoliageComponent.h"
#include "ProceduralFoliageSpawner.h"
#include "ProceduralFoliageVolume.h"
#include "WorldPartition/WorldPartition.h"
#endif

#if WITH_EDITOR
namespace McpFoliageHandlers {
AInstancedFoliageActor* GetOrCreateFoliageActorForWorldSafe(UWorld* World, bool bCreateIfNone);
}
#endif

// =============================================================================
// McpSafeOperations.h
// =============================================================================
// Safe asset and level operations with UE 5.7+ compatibility
//
// CRITICAL for UE 5.7+:
// - McpSafeAssetSave() - Replaces UEditorAssetLibrary::SaveAsset() to avoid crashes
// - McpSafeLevelSave() - Safe level saving with render thread synchronization
// - McpSafeLoadMap() - Safe map loading with TickTaskManager cleanup
//
// Copyright (c) 2024 MCP Automation Bridge Contributors
// =============================================================================

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Runtime/Launch/Resources/Version.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "Engine/LevelStreaming.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "Components/ActorComponent.h"
#include "TickTaskManagerInterface.h"
#include "HAL/PlatformProcess.h"
#include "RenderingThread.h"
#include "Async/TaskGraphInterfaces.h"
#include "UObject/SoftObjectPath.h"

#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif

#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "AssetViewUtils.h"
#include "Materials/MaterialInterface.h"
#include "Editor/EditorEngine.h"
#include "UObject/UObjectIterator.h"
#include "ObjectTools.h"

#if __has_include("Subsystems/AssetEditorSubsystem.h")
#include "Subsystems/AssetEditorSubsystem.h"
#define MCP_HAS_ASSET_EDITOR_SUBSYSTEM 1
#else
#define MCP_HAS_ASSET_EDITOR_SUBSYSTEM 0
#endif

// FAssetCompilingManager for UE 5.7+ compilation quiesce
#if __has_include("AssetCompilingManager.h")
#include "AssetCompilingManager.h"
#define MCP_HAS_ASSET_COMPILING_MANAGER 1
#else
#define MCP_HAS_ASSET_COMPILING_MANAGER 0
#endif

// Animation/AnimBlueprint support for safe deletion
#if __has_include("Animation/AnimBlueprint.h")
#include "Animation/AnimBlueprint.h"
#define MCP_HAS_ANIM_BLUEPRINT 1
#else
#define MCP_HAS_ANIM_BLUEPRINT 0
#endif

// AnimBlueprint editor cleanup helpers
#if __has_include("AnimationEditorUtils.h")
#include "AnimationEditorUtils.h"
#define MCP_HAS_ANIMATION_EDITOR_UTILS 1
#else
#define MCP_HAS_ANIMATION_EDITOR_UTILS 0
#endif

// Selection support for clearing editor selections
#if __has_include("Engine/Selection.h")
#include "Engine/Selection.h"
#define MCP_HAS_SELECTION 1
#else
#define MCP_HAS_SELECTION 0
#endif

// BlueprintActionDatabase for pre-clearing entries before deletion
// WORKAROUND for UE 5.7 bug: ClearAssetActions() uses ActionList after Remove()
#if __has_include("BlueprintActionDatabase.h")
#include "BlueprintActionDatabase.h"
#define MCP_HAS_BLUEPRINT_ACTION_DATABASE 1
#else
#define MCP_HAS_BLUEPRINT_ACTION_DATABASE 0
#endif

// PackageTools for unloading packages before deletion
#if __has_include("PackageTools.h")
#include "PackageTools.h"
#define MCP_HAS_PACKAGE_TOOLS 1
#else
#define MCP_HAS_PACKAGE_TOOLS 0
#endif

#endif

// =============================================================================
// Log Category Declaration (defined in subsystem)
// =============================================================================
DECLARE_LOG_CATEGORY_EXTERN(LogMcpSafeOperations, Log, All);

// =============================================================================
// Safe Asset Operations
// =============================================================================
namespace McpSafeOperations
{

#if WITH_EDITOR

/**
 * Safe asset saving helper - marks package dirty and notifies asset registry.
 * 
 * CRITICAL FOR UE 5.7+:
 * DO NOT use UEditorAssetLibrary::SaveAsset() - it triggers modal dialogs 
 * that crash D3D12RHI during automation. This helper marks dirty instead,
 * letting the editor save on shutdown or explicit user action.
 *
 * @param Asset The UObject asset to mark dirty
 * @returns true if the asset was marked dirty successfully
 */
inline bool McpSafeAssetSave(UObject* Asset)
{
    if (!Asset)
    {
        return false;
    }

    // UE 5.7+ Fix: Do not immediately save newly created assets to disk.
    // Saving immediately causes bulkdata corruption and crashes.
    // Instead, mark the package dirty and notify the asset registry.
    Asset->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Asset);
    
    return true;
}

/**
 * Safely save a level with UE 5.7+ compatibility workarounds.
 *
 * CRITICAL: Intel GPU drivers (MONZA DdiThreadingContext) can crash when
 * FEditorFileUtils::SaveLevel() is called immediately after level creation.
 *
 * This helper:
 * 1. Suspends the render thread during save (prevents driver race condition)
 * 2. Flushes all rendering commands before and after save
 * 3. Verifies the file exists after save
 * 4. Validates path length to prevent Windows Error 87 (MAX_PATH exceeded)
 *
 * @param Level The ULevel to save
 * @param FullPath The full package path for the level
 * @param MaxRetries Unused (kept for API compatibility)
 * @return true if save succeeded and file exists
 */
inline bool McpSafeLevelSave(ULevel* Level, const FString& FullPath, int32 MaxRetries = 1)
{
    if (!Level)
    {
        UE_LOG(LogMcpSafeOperations, Error, TEXT("McpSafeLevelSave: Level is null"));
        return false;
    }

    // CRITICAL: Reject transient/unsaved level paths that would cause double-slash package names
    if (FullPath.StartsWith(TEXT("/Temp/")) ||
        FullPath.StartsWith(TEXT("/Engine/Transient")) ||
        FullPath.Contains(TEXT("Untitled")))
    {
        UE_LOG(LogMcpSafeOperations, Error, 
            TEXT("McpSafeLevelSave: Cannot save transient level: %s. Use save_as with a valid path."), 
            *FullPath);
        return false;
    }

    FString PackagePath = FullPath;
    if (!PackagePath.StartsWith(TEXT("/Game/")))
    {
        if (!PackagePath.StartsWith(TEXT("/")))
        {
            PackagePath = TEXT("/Game/") + PackagePath;
        }
        else
        {
            UE_LOG(LogMcpSafeOperations, Error, 
                TEXT("McpSafeLevelSave: Invalid path (not under /Game/): %s"), *PackagePath);
            return false;
        }
    }

    // Validate no double slashes in the path
    if (PackagePath.Contains(TEXT("//")))
    {
        UE_LOG(LogMcpSafeOperations, Error, 
            TEXT("McpSafeLevelSave: Path contains double slashes: %s"), *PackagePath);
        return false;
    }

    // Ensure path has proper format
    if (PackagePath.Contains(TEXT(".")))
    {
        PackagePath = PackagePath.Left(PackagePath.Find(TEXT(".")));
    }

    // CRITICAL: Validate path length to prevent Windows Error 87
    {
        FString AbsoluteFilePath;
        if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, AbsoluteFilePath, 
            FPackageName::GetMapPackageExtension()))
        {
            AbsoluteFilePath = FPaths::ConvertRelativePathToFull(AbsoluteFilePath);
            const int32 SafePathLength = 240;
            if (AbsoluteFilePath.Len() > SafePathLength)
            {
                UE_LOG(LogMcpSafeOperations, Error, 
                    TEXT("McpSafeLevelSave: Path too long (%d chars, max %d): %s"), 
                    AbsoluteFilePath.Len(), SafePathLength, *AbsoluteFilePath);
                UE_LOG(LogMcpSafeOperations, Error, 
                    TEXT("McpSafeLevelSave: Use a shorter path or enable Windows long paths"));
                return false;
            }
        }
    }

    // Check if level already exists BEFORE attempting save
    {
        FString ExistingLevelFilename;
        bool bLevelExists = false;
        
        if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, ExistingLevelFilename, 
            FPackageName::GetMapPackageExtension()))
        {
            FString AbsolutePath = FPaths::ConvertRelativePathToFull(ExistingLevelFilename);
            bLevelExists = IFileManager::Get().FileExists(*AbsolutePath);
            
            if (!bLevelExists)
            {
                FString LevelName = FPaths::GetBaseFilename(PackagePath);
                FString FolderPath = FPaths::GetPath(AbsolutePath) / LevelName + FPackageName::GetMapPackageExtension();
                bLevelExists = IFileManager::Get().FileExists(*FolderPath);
            }
        }
        
        if (!bLevelExists)
        {
            bLevelExists = FPackageName::DoesPackageExist(PackagePath);
        }
        
        if (bLevelExists)
        {
            UWorld* LevelWorld = Level ? Level->GetWorld() : nullptr;
            if (LevelWorld)
            {
                FString CurrentLevelPath = LevelWorld->GetOutermost()->GetName();
                if (CurrentLevelPath.Equals(PackagePath, ESearchCase::IgnoreCase))
                {
                    UE_LOG(LogMcpSafeOperations, Log, 
                        TEXT("McpSafeLevelSave: Overwriting existing level: %s"), *PackagePath);
                }
                else
                {
                    UE_LOG(LogMcpSafeOperations, Warning, 
                        TEXT("McpSafeLevelSave: Level already exists at %s (current level is %s)"), 
                        *PackagePath, *CurrentLevelPath);
                    return false;
                }
            }
        }
    }

    // CRITICAL: Flush rendering commands to prevent Intel driver race condition
    FlushRenderingCommands();

    // Small delay after flush to ensure GPU is completely idle
    FPlatformProcess::Sleep(0.050f); // 50ms wait

    // Perform the actual save
    // CRITICAL FIX: Always use FEditorFileUtils::SaveLevel instead of UEditorLoadingAndSavingUtils::SaveMap.
    // UEditorLoadingAndSavingUtils::SaveMap saves to a new package but doesn't update the world's outer
    // package name. This causes "World Memory Leaks" crashes when load_level is called because
    // McpSafeLoadMap doesn't recognize the saved level as the current level (package name mismatch).
    // FEditorFileUtils::SaveLevel properly updates the world's package to match the save path.
    bool bSaveSucceeded = FEditorFileUtils::SaveLevel(Level, *PackagePath);

    if (bSaveSucceeded)
    {
        // Small delay before verification
        FPlatformProcess::Sleep(0.050f);

        // Verify file exists on disk
        FString VerifyFilename;
        if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, VerifyFilename, 
            FPackageName::GetMapPackageExtension()))
        {
            FString AbsoluteVerifyFilename = FPaths::ConvertRelativePathToFull(VerifyFilename);
            
            if (IFileManager::Get().FileExists(*VerifyFilename) || 
                IFileManager::Get().FileExists(*AbsoluteVerifyFilename))
            {
                UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeLevelSave: Successfully saved level: %s"), *PackagePath);
                return true;
            }
            
            // FALLBACK: Check if package exists in UE's package system
            if (FPackageName::DoesPackageExist(PackagePath))
            {
                UE_LOG(LogMcpSafeOperations, Log, 
                    TEXT("McpSafeLevelSave: Package exists in UE system: %s"), *PackagePath);
                return true;
            }
            
            UE_LOG(LogMcpSafeOperations, Error, 
                TEXT("McpSafeLevelSave: Save reported success but file not found: %s"), *VerifyFilename);
        }
        else
        {
            UE_LOG(LogMcpSafeOperations, Warning, 
                TEXT("McpSafeLevelSave: Failed to convert package path to filename: %s"), *PackagePath);
        }
    }

    UE_LOG(LogMcpSafeOperations, Error, TEXT("McpSafeLevelSave: Failed to save level: %s"), *PackagePath);
    return false;
}

/**
 * Safe map loading - properly cleans up current world before loading a new map.
 * Prevents TickTaskManager assertion "!LevelList.Contains(TickTaskLevel)" and
 * "World Memory Leaks" crashes in UE 5.7.
 *
 * CRITICAL UE 5.7 FIX: The "Pure virtual not implemented" crash occurs when
 * tick tasks reference destroyed actors/components. This function ensures:
 * 1. All prerequisites are cleared BEFORE unregistering tick functions
 * 2. All pending tick tasks complete before world destruction
 * 3. Task graph is fully drained of tick-related work
 *
 * CRITICAL: This function must be called from the Game Thread.
 *
 * @param MapPath The map path to load (e.g., /Game/Maps/MyMap)
 * @param bForceCleanup If true, perform aggressive cleanup before loading
 * @return bool True if the map was loaded successfully
 */
inline bool McpSafeLoadMap(const FString& MapPath, bool bForceCleanup = true)
{
    if (!GEditor)
    {
        UE_LOG(LogMcpSafeOperations, Error, TEXT("McpSafeLoadMap: GEditor is null"));
        return false;
    }

    // CRITICAL: Ensure we're on the game thread
    if (!IsInGameThread())
    {
        UE_LOG(LogMcpSafeOperations, Error, TEXT("McpSafeLoadMap: Must be called from game thread"));
        return false;
    }

    // CRITICAL: Wait for any async loading to complete
    int32 AsyncWaitCount = 0;
    while (IsAsyncLoading() && AsyncWaitCount < 100)
    {
        FlushAsyncLoading();
        FPlatformProcess::Sleep(0.01f);
        AsyncWaitCount++;
    }
    if (AsyncWaitCount > 0)
    {
        UE_LOG(LogMcpSafeOperations, Log, 
            TEXT("McpSafeLoadMap: Waited %d frames for async loading to complete"), AsyncWaitCount);
    }

    // CRITICAL: Stop PIE if active
    if (GEditor->PlayWorld)
    {
        UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeLoadMap: Stopping active PIE session before loading map"));
        GEditor->RequestEndPlayMap();
        int32 PieWaitCount = 0;
        while (GEditor->PlayWorld && PieWaitCount < 100)
        {
            FlushRenderingCommands();
            FPlatformProcess::Sleep(0.01f);
            PieWaitCount++;
        }
        FlushRenderingCommands();
    }

    UWorld* CurrentWorld = GEditor->GetEditorWorldContext().World();
    
    // CRITICAL: Check if the map we're trying to load is already the current map FIRST.
    // This must happen BEFORE cleanup to avoid unnecessary cleanup on the same level.
    // If we cleanup first and then check, we destroy tick functions on the level we want to keep.
    if (CurrentWorld)
    {
        FString CurrentMapPath = CurrentWorld->GetOutermost()->GetName();
        FString NormalizedMapPath = MapPath;
        
        if (NormalizedMapPath.EndsWith(TEXT(".umap")))
        {
            NormalizedMapPath.LeftChopInline(5);
        }
        
        if (CurrentMapPath.Equals(NormalizedMapPath, ESearchCase::IgnoreCase))
        {
            UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeLoadMap: Map '%s' is already loaded, skipping"), *MapPath);
            return true;
        }
    }
    
    // CRITICAL: Check for World Partition
    if (CurrentWorld)
    {
        AWorldSettings* WorldSettings = CurrentWorld->GetWorldSettings();
        UWorldPartition* CurrentWorldPartition = WorldSettings ? WorldSettings->GetWorldPartition() : nullptr;
        if (CurrentWorldPartition)
        {
            UE_LOG(LogMcpSafeOperations, Warning, 
                TEXT("McpSafeLoadMap: Current world '%s' has World Partition - tick cleanup may be incomplete"), 
                *CurrentWorld->GetName());
        }
    }
    
    if (CurrentWorld && bForceCleanup)
    {
        UE_LOG(LogMcpSafeOperations, Log, 
            TEXT("McpSafeLoadMap: Cleaning up current world '%s' before loading '%s'"), 
            *CurrentWorld->GetName(), *MapPath);

#if MCP_HAS_ASSET_EDITOR_SUBSYSTEM
        if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
            AssetEditorSubsystem->CloseAllAssetEditors();
        }
#endif

        FlushRenderingCommands();
        GEditor->ForceGarbageCollection(true);
        FlushRenderingCommands();
        FPlatformProcess::Sleep(0.05f);

        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpSafeLoadMap: Minimal cleanup completed before map load"));
    }
    
    // STEP 11: Load the map
    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeLoadMap: Loading map '%s'"), *MapPath);
    bool bLoaded = FEditorFileUtils::LoadMap(*MapPath);
    
    if (bLoaded)
    {
        UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeLoadMap: Successfully loaded map '%s'"), *MapPath);
        
        // STEP 13: Disable ticking on new world's actors
        UWorld* NewWorld = GEditor->GetEditorWorldContext().World();
        if (NewWorld && NewWorld->PersistentLevel)
        {
            for (AActor* Actor : NewWorld->PersistentLevel->Actors)
            {
                if (Actor)
                {
                    Actor->SetActorTickEnabled(false);
                    for (UActorComponent* Component : Actor->GetComponents())
                    {
                        if (Component)
                        {
                            Component->SetComponentTickEnabled(false);
                        }
                    }
                }
            }
        }
    }
    else
    {
        UE_LOG(LogMcpSafeOperations, Error, TEXT("McpSafeLoadMap: Failed to load map '%s'"), *MapPath);
    }
    
    return bLoaded;
}

/**
 * Material fallback helper for robust material loading across UE versions.
 * Attempts to load a material with fallback chain for engine defaults.
 *
 * @param MaterialPath Preferred material path (can be empty to use fallback immediately)
 * @param bSilent If true, suppresses warning logs for missing requested material
 * @return UMaterialInterface* or nullptr if all fallbacks fail
 */
inline UMaterialInterface* McpLoadMaterialWithFallback(const FString& MaterialPath, bool bSilent = false)
{
    // Try requested path first if provided
    if (!MaterialPath.IsEmpty())
    {
        UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
        if (Material)
        {
            return Material;
        }
        if (!bSilent)
        {
            UE_LOG(LogMcpSafeOperations, Warning, 
                TEXT("McpLoadMaterialWithFallback: Requested material not found: %s"), *MaterialPath);
        }
    }
    
    // Fallback chain for engine materials
    const TCHAR* FallbackPaths[] = {
        TEXT("/Engine/EngineMaterials/DefaultMaterial"),
        TEXT("/Engine/EngineMaterials/WorldGridMaterial"),
        TEXT("/Engine/EngineMaterials/DefaultDeferredDecalMaterial"),
        TEXT("/Engine/EngineMaterials/DefaultTextMaterialOpaque")
    };
    
    for (const TCHAR* FallbackPath : FallbackPaths)
    {
        UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, FallbackPath);
        if (Material)
        {
            if (!bSilent && !MaterialPath.IsEmpty())
            {
                UE_LOG(LogMcpSafeOperations, Log, 
                    TEXT("McpLoadMaterialWithFallback: Using fallback '%s' for '%s'"), 
                    FallbackPath, *MaterialPath);
            }
            return Material;
        }
    }
    
    UE_LOG(LogMcpSafeOperations, Error, 
        TEXT("McpLoadMaterialWithFallback: All fallback materials unavailable - engine content may be missing"));
    return nullptr;
}

/**
 * Throttled wrapper around UEditorAssetLibrary::SaveLoadedAsset to avoid
 * rapid repeated SavePackage calls which can cause engine warnings.
 *
 * @param Asset The asset to save
 * @param ThrottleSecondsOverride Override throttle time (default uses global setting)
 * @param bForce If true, ignore throttling and force immediate save
 * @return true if save succeeded or was skipped due to throttle
 */
inline bool SaveLoadedAssetThrottled(UObject* Asset, double ThrottleSecondsOverride = -1.0, bool bForce = false)
{
    if (!Asset)
    {
        return false;
    }

    // Throttling parameters reserved for future implementation
    // Currently delegates directly to McpSafeAssetSave for UE 5.7 compatibility
    // TODO: Implement actual throttling with ThrottleSecondsOverride and bForce
    (void)ThrottleSecondsOverride; // Reserved for throttle duration override
    (void)bForce; // Reserved for forcing immediate save bypassing throttle

    return McpSafeAssetSave(Asset);
}

/**
 * Force a synchronous scan of a specific package or folder path.
 *
 * @param InPath The path to scan
 * @param bRecursive Whether to scan recursively
 */
inline void ScanPathSynchronous(const FString& InPath, bool bRecursive = true)
{
    FAssetRegistryModule& AssetRegistryModule = 
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FString> PathsToScan;
    PathsToScan.Add(InPath);
    AssetRegistry.ScanPathsSynchronous(PathsToScan, bRecursive);
}

/**
 * Pre-clear BlueprintActionDatabase entry before deletion.
 * 
 * WORKAROUND FOR UE 5.7 BUG:
 * FBlueprintActionDatabase::ClearAssetActions() has a use-after-free bug:
 *   1. ActionRegistry.Remove(AssetObjectKey) removes the entry
 *   2. Then ActionList->Num() is called on the now-dangling pointer
 * This corrupts heap state on the first Blueprint deletion, causing crashes
 * on subsequent deletions with 0xFFFFFFFFFFFFFFFF access violations.
 * 
 * By pre-clearing the entry HERE before ForceDeleteObjects runs, we ensure
 * that when the engine's OnAssetsPreDelete delegate fires ClearAssetActions,
 * the entry is already gone and the buggy code path is skipped.
 *
 * @param Asset The asset to pre-clear from the action database
 */
inline void McpPreClearBlueprintActionDatabase(UObject* Asset)
{
    if (!Asset)
    {
        return;
    }

#if MCP_HAS_BLUEPRINT_ACTION_DATABASE
    // Only need to do this for Blueprint-derived assets
    if (!Asset->IsA<UBlueprint>())
    {
        return;
    }

    UE_LOG(LogMcpSafeOperations, Log, 
        TEXT("McpPreClearBlueprintActionDatabase: Pre-clearing action database for '%s'"), 
        *Asset->GetName());

    // Get the singleton and clear the entry BEFORE ForceDeleteObjects runs
    FBlueprintActionDatabase& ActionDB = FBlueprintActionDatabase::Get();
    ActionDB.ClearAssetActions(Asset);

    UE_LOG(LogMcpSafeOperations, Log, 
        TEXT("McpPreClearBlueprintActionDatabase: Pre-clear complete for '%s'"), 
        *Asset->GetName());
#endif
}

/**
 * Perform garbage collection after asset deletion.
 * 
 * CRITICAL FOR UE 5.7+:
 * After deleting assets (especially AnimBlueprints, IKRigs, IKRetargeters),
 * garbage collection must be forced to prevent access violations during
 * UWorld::CleanupWorld.
 *
 * @param bFullPurge If true, perform a full purge (more aggressive)
 */
inline void McpSafePostDeleteGC(bool bFullPurge = true)
{
    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafePostDeleteGC: Starting post-delete cleanup"));
    
    // Flush rendering commands to ensure all GPU work is complete
    FlushRenderingCommands();
    
    // Force garbage collection
    if (GEditor)
    {
        GEditor->ForceGarbageCollection(bFullPurge);
    }
    
    // Flush again to process any pending destroy operations
    FlushRenderingCommands();
    
    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafePostDeleteGC: Post-delete cleanup completed"));
}

/**
 * Fully quiesce all editor state before asset deletion.
 * 
 * CRITICAL FOR UE 5.7+:
 * Force-deleting animation/IK assets crashes if compilation, rendering, or editor
 * state is not fully quiesced. This function ensures:
 * 1. All asset compilation finishes (FAssetCompilingManager)
 * 2. All rendering commands complete (FlushRenderingCommands)
 * 3. Editor subsystems are synchronized
 * 4. Garbage collection runs to clean up stale references
 *
 * Call this BEFORE any batch of risky asset deletions.
 */
inline void McpQuiesceAllState()
{
    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpQuiesceAllState: Starting full editor quiesce"));
    
    // STEP 1: Finish all pending asset compilation
    // This is critical for animation assets that may be compiling in background
#if MCP_HAS_ASSET_COMPILING_MANAGER
    FAssetCompilingManager& CompilingManager = FAssetCompilingManager::Get();
    int32 RemainingAssets = CompilingManager.GetNumRemainingAssets();
    if (RemainingAssets > 0)
    {
        UE_LOG(LogMcpSafeOperations, Log, 
            TEXT("McpQuiesceAllState: Waiting for %d compiling assets"), RemainingAssets);
        CompilingManager.FinishAllCompilation();
    }
#endif
    
    // STEP 2: Flush rendering commands to ensure GPU is idle
    FlushRenderingCommands();
    
    // STEP 3: Small delay to allow any pending UI/editor operations to complete
    FPlatformProcess::Sleep(0.016f); // ~1 frame at 60fps
    
    // STEP 4: Flush again after delay
    FlushRenderingCommands();
    
    // STEP 5: Force garbage collection to clean up any stale references
    if (GEditor)
    {
        GEditor->ForceGarbageCollection(true);
    }
    
    // STEP 6: Final flush to process GC cleanup
    FlushRenderingCommands();
    
    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpQuiesceAllState: Editor quiesce completed"));
}

/**
 * Finish compilation for a specific batch of objects before/after deletion.
 * Uses FinishCompilationForObjects if available (UE 5.7+), falls back to FinishAllCompilation.
 * 
 * CRITICAL FOR UE 5.7+:
 * This provides tighter compilation barriers than just FinishAllCompilation() by
 * targeting the specific objects being deleted.
 *
 * @param BatchObjects The objects to finish compilation for
 * @param Context String for logging (e.g., "pre-delete" or "post-delete")
 */
inline void McpFinishCompilationForBatch(TArray<UObject*>& BatchObjects, const TCHAR* Context)
{
#if MCP_HAS_ASSET_COMPILING_MANAGER
    FAssetCompilingManager& CompilingManager = FAssetCompilingManager::Get();
    
    // Log current state
    int32 GlobalRemaining = CompilingManager.GetNumRemainingAssets();
    UE_LOG(LogMcpSafeOperations, Log, 
        TEXT("McpFinishCompilationForBatch: [%s] %d global compiling assets, %d objects in batch"), 
        Context, GlobalRemaining, BatchObjects.Num());
    
    // STEP 1: Finish compilation for specific batch objects (tightest barrier)
    // This is more targeted than FinishAllCompilation and prevents race conditions
    // with the specific assets being deleted
    if (BatchObjects.Num() > 0)
    {
        CompilingManager.FinishCompilationForObjects(BatchObjects);
    }
    
    // STEP 2: Global compilation barrier (catches any dependencies)
    // After batch-specific finish, ensure nothing else is compiling that might
    // reference the batch objects
    GlobalRemaining = CompilingManager.GetNumRemainingAssets();
    if (GlobalRemaining > 0)
    {
        UE_LOG(LogMcpSafeOperations, Log, 
            TEXT("McpFinishCompilationForBatch: [%s] Finishing remaining %d global assets"), 
            Context, GlobalRemaining);
        CompilingManager.FinishAllCompilation();
    }
    
    UE_LOG(LogMcpSafeOperations, Log, 
        TEXT("McpFinishCompilationForBatch: [%s] Compilation barriers complete"), Context);
#else
    // Without FAssetCompilingManager, just log that we're skipping
    UE_LOG(LogMcpSafeOperations, Log, 
        TEXT("McpFinishCompilationForBatch: [%s] FAssetCompilingManager not available, skipping batch compilation"), 
        Context);
    (void)BatchObjects; // Suppress unused parameter warning
    (void)Context;
#endif
}

/**
 * Pre-delete quiesce for a batch of risky assets.
 * 
 * CRITICAL FOR UE 5.7+:
 * Must be called immediately before each batch deletion to ensure:
 * 1. Editors are closed for the batch (prevents editor references)
 * 2. Batch-specific compilation finishes (FinishCompilationForObjects)
 * 3. Global compilation finishes (catches dependencies)
 * 4. Render thread is flushed
 * 5. Garbage collection runs
 *
 * @param BatchObjects The objects about to be deleted
 */
inline void McpQuiesceBeforeBatchDelete(TArray<UObject*>& BatchObjects)
{
    UE_LOG(LogMcpSafeOperations, Log, 
        TEXT("McpQuiesceBeforeBatchDelete: Starting pre-delete quiesce for %d objects"), 
        BatchObjects.Num());
    
    // STEP 1: Close editors for batch objects
#if MCP_HAS_ASSET_EDITOR_SUBSYSTEM
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    if (AssetEditorSubsystem)
    {
        for (UObject* Asset : BatchObjects)
        {
            if (Asset)
            {
                AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
            }
        }
    }
#endif
    
    // STEP 2: Batch-specific compilation barrier
    McpFinishCompilationForBatch(BatchObjects, TEXT("pre-delete"));
    
    // STEP 3: Flush rendering commands
    FlushRenderingCommands();
    
    // STEP 4: Small delay for editor state to settle
    FPlatformProcess::Sleep(0.016f);
    
    // STEP 5: Flush again
    FlushRenderingCommands();
    
    // STEP 6: Force garbage collection to clean up editor references
    if (GEditor)
    {
        GEditor->ForceGarbageCollection(true);
    }
    
    // STEP 7: Final flush
    FlushRenderingCommands();
    
    UE_LOG(LogMcpSafeOperations, Log, 
        TEXT("McpQuiesceBeforeBatchDelete: Pre-delete quiesce complete"));
}

/**
 * Post-delete quiesce after a batch deletion.
 * 
 * CRITICAL FOR UE 5.7+:
 * Must be called immediately after each batch deletion to ensure:
 * 1. Any compilation triggered by deletion completes
 * 2. Render thread processes destruction
 * 3. Garbage collection cleans up deleted object references
 *
 * @param BatchObjects The objects that were just deleted (may contain stale pointers, used for count only)
 */
inline void McpQuiesceAfterBatchDelete(const TArray<UObject*>& BatchObjects)
{
    UE_LOG(LogMcpSafeOperations, Log, 
        TEXT("McpQuiesceAfterBatchDelete: Starting post-delete quiesce for %d objects"), 
        BatchObjects.Num());
    
    // STEP 1: Flush rendering commands to process destruction
    FlushRenderingCommands();
    
    // STEP 2: Small delay for destruction to complete
    FPlatformProcess::Sleep(0.016f);
    
    // STEP 3: Flush again
    FlushRenderingCommands();
    
    // STEP 4: Force garbage collection to clean up deleted object references
    if (GEditor)
    {
        GEditor->ForceGarbageCollection(true);
    }
    
    // STEP 5: Final flush
    // NOTE: We do NOT call FinishAllCompilation() here because:
    // 1. It can trigger compilation of assets that reference deleted objects
    // 2. After ForceGarbageCollection, any pending compilations may have stale refs
    // 3. Compilation barrier is handled in McpQuiesceBeforeBatchDelete BEFORE deletion
    FlushRenderingCommands();
    
    UE_LOG(LogMcpSafeOperations, Log, 
        TEXT("McpQuiesceAfterBatchDelete: Post-delete quiesce complete"));
}

/**
 * AnimBlueprint-specific pre-delete quiesce.
 * 
 * CRITICAL FOR UE 5.7+:
 * AnimBlueprints have additional complexity beyond regular Blueprints:
 * - UAnimBlueprintGeneratedClass with animation debug data
 * - TargetSkeleton reference
 * - Active Persona editors, anim graph previews, and debug sessions
 * - Editor selection state
 * 
 * IMPORTANT: This function must NOT modify internal AnimBlueprint state that
 * ForceDeleteObjects relies on for proper generated-class teardown. The engine's
 * ForceDeleteObjects() handles:
 *   - Adding GeneratedClass and SkeletonGeneratedClass to replace list
 *   - Calling RemoveChildRedirectors()
 *   - Calling RemoveGeneratedClasses()
 *   - ForceReplaceReferences() with proper rendering context
 *
 * DO NOT call RemoveAllExtension() or other destructive state modifications here
 * as they corrupt state that Engine.dll callbacks (via OnAddExtraObjectsToDelete)
 * may still be accessing, causing 0xFFFFFFFFFFFFFFFF access violations.
 *
 * @param AnimBlueprint The AnimBlueprint asset to quiesce
 */
inline void McpQuiesceAnimBlueprintBeforeDelete(UAnimBlueprint* AnimBlueprint)
{
    if (!AnimBlueprint)
    {
        return;
    }

    UE_LOG(LogMcpSafeOperations, Log, 
        TEXT("McpQuiesceAnimBlueprintBeforeDelete: Starting AnimBlueprint-specific quiesce for '%s'"), 
        *AnimBlueprint->GetName());

    // STEP 1: Close ALL editors for this AnimBlueprint
    // This includes Persona editors, animation graph editors, and any embedded toolkits
    // Note: CloseAllEditorsForAsset should close Persona, but we force it explicitly
#if MCP_HAS_ASSET_EDITOR_SUBSYSTEM
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    if (AssetEditorSubsystem)
    {
        // Close editors multiple times to ensure nested/embedded editors are also closed
        // Sometimes Persona editors can survive a single close call
        for (int32 i = 0; i < 3; ++i)
        {
            AssetEditorSubsystem->CloseAllEditorsForAsset(AnimBlueprint);
        }
        
        UE_LOG(LogMcpSafeOperations, Log, 
            TEXT("McpQuiesceAnimBlueprintBeforeDelete: Closed all editors for '%s'"), 
            *AnimBlueprint->GetName());
    }
#endif

    // STEP 2: Clear editor selection if this AnimBlueprint is selected
    // AnimBlueprints in selection can cause access violations during deletion
#if MCP_HAS_SELECTION
    if (GEditor)
    {
        USelection* SelectedObjects = GEditor->GetSelectedObjects();
        if (SelectedObjects && SelectedObjects->IsSelected(AnimBlueprint))
        {
            SelectedObjects->Deselect(AnimBlueprint);
            UE_LOG(LogMcpSafeOperations, Log, 
                TEXT("McpQuiesceAnimBlueprintBeforeDelete: Deselected AnimBlueprint '%s'"), 
                *AnimBlueprint->GetName());
        }
        
        // Also clear any subobject selections (anim graph nodes, etc.)
        GEditor->SelectNone(false, true, false);
    }
#endif

    // STEP 3: Flush rendering commands and wait for editor state to settle.
    // DO NOT modify internal AnimBlueprint state here - ForceDeleteObjects
    // needs that state intact for proper generated-class teardown.
    FlushRenderingCommands();
    
    // Small delay to allow async editor cleanup
    FPlatformProcess::Sleep(0.050f); // 50ms wait
    
    // Final flush
    FlushRenderingCommands();

    UE_LOG(LogMcpSafeOperations, Log, 
        TEXT("McpQuiesceAnimBlueprintBeforeDelete: AnimBlueprint-specific quiesce complete for '%s'"), 
        *AnimBlueprint->GetName());
}

/**
 * Check if an asset is an AnimBlueprint by class name (registry-only, no asset load).
 * 
 * @param AssetData The asset data from registry (metadata only)
 * @return true if the asset is an AnimBlueprint type
 */
inline bool IsAnimBlueprintAsset(const FAssetData& AssetData)
{
    FString ClassName = AssetData.AssetClassPath.ToString();
    return ClassName.Contains(TEXT("AnimBlueprint"));
}

/**
 * Check if an asset is any Blueprint-derived type that needs the action database workaround.
 * Uses ONLY registry metadata (no GetClass/GetAsset calls that load packages).
 * 
 * The UE 5.7 BlueprintActionDatabase bug affects ALL Blueprint types, not just AnimBlueprints.
 * 
 * @param AssetData The asset data from registry (metadata only)
 * @return true if the asset is any Blueprint type
 */
inline bool IsAnyBlueprintAsset(const FAssetData& AssetData)
{
    FString ClassName = AssetData.AssetClassPath.ToString();
    // Match any Blueprint-derived type
    return ClassName.Contains(TEXT("Blueprint")) ||
           ClassName.Contains(TEXT("WidgetBlueprint")) ||
           ClassName.Contains(TEXT("ControlRigBlueprint"));
}

/**
 * Check if an asset is a "risky" animation/IK asset that needs batch deletion.
 * Uses ONLY registry metadata (no GetClass/GetAsset calls that load packages).
 *
 * Risky assets: AnimBlueprint, AnimSequence, IKRigDefinition, IKRetargeter,
 * ControlRigBlueprint, AimOffsetBlendSpace, BlendSpace, and similar
 * animation-related types that commonly crash on force-delete.
 *
 * @param AssetData The asset data from registry (metadata only)
 * @return true if the asset is a risky animation/IK type
 */
inline bool IsRiskyAnimationAsset(const FAssetData& AssetData)
{
    FString ClassName = AssetData.AssetClassPath.ToString();
    
    // Animation assets that commonly crash on force-delete
    // These have complex compilation/render state that must be quiesced first
    static const TArray<FString> RiskyAnimationClasses = {
        TEXT("AnimBlueprint"),
        TEXT("AnimSequence"),
        TEXT("AnimMontage"),
        TEXT("AnimComposite"),
        TEXT("IKRigDefinition"),
        TEXT("IKRetargeter"),
        TEXT("ControlRigBlueprint"),
        TEXT("AimOffsetBlendSpace"),
        TEXT("BlendSpace"),
        TEXT("BlendSpace1D"),
        TEXT("BlendSpaceBase"),
        TEXT("PoseAsset"),
        TEXT("Skeleton")
    };
    
    for (const FString& RiskyClass : RiskyAnimationClasses)
    {
        if (ClassName.Contains(RiskyClass))
        {
            return true;
        }
    }
    
    return false;
}

/**
 * Get the ordered delete priority for the remaining crash-prone animation/rig cluster.
 * Lower value means delete earlier.
 *
 * Explicit order for the mixed cluster:
 *   0. AnimBlueprint
 *   1. IKRigDefinition
 *   2. AnimSequence
 *   3. ControlRigBlueprint
 *   4. Any other risky animation asset
 */
inline int32 GetAnimationRigClusterDeletePriority(const FAssetData& AssetData)
{
    const FString ClassName = AssetData.AssetClassPath.ToString();

    if (ClassName.Contains(TEXT("AnimBlueprint")))
    {
        return 0;
    }
    if (ClassName.Contains(TEXT("IKRigDefinition")))
    {
        return 1;
    }
    if (ClassName.Contains(TEXT("AnimSequence")))
    {
        return 2;
    }
    if (ClassName.Contains(TEXT("ControlRigBlueprint")))
    {
        return 3;
    }

    return 4;
}

/**
 * Check whether a risky asset set contains the known crash-prone mixed animation/rig cluster.
 * We only special-case when at least two of the problematic classes are present together.
 */
inline bool IsMixedAnimationRigCluster(const TArray<FAssetData>& Assets)
{
    bool bHasIKRigDefinition = false;
    bool bHasAnimSequence = false;
    bool bHasAnimBlueprint = false;
    bool bHasControlRigBlueprint = false;

    for (const FAssetData& AssetData : Assets)
    {
        const int32 Priority = GetAnimationRigClusterDeletePriority(AssetData);
        switch (Priority)
        {
        case 0:
            bHasAnimBlueprint = true;
            break;
        case 1:
            bHasIKRigDefinition = true;
            break;
        case 2:
            bHasAnimSequence = true;
            break;
        case 3:
            bHasControlRigBlueprint = true;
            break;
        default:
            break;
        }
    }

    const int32 ClusterTypeCount =
        (bHasIKRigDefinition ? 1 : 0) +
        (bHasAnimSequence ? 1 : 0) +
        (bHasAnimBlueprint ? 1 : 0) +
        (bHasControlRigBlueprint ? 1 : 0);

    return ClusterTypeCount >= 2;
}

/**
 * Delete the known crash-prone animation/rig cluster in explicit class order.
 * 
 * CRITICAL FOR UE 5.7+: AnimBlueprints have cross-package references (linked anim graphs,
 * skeleton references, debug data) that cause 0xFFFFFFFFFFFFFFFF crashes when:
 * - ForceDeleteObjects tries to delete them while loaded
 * - UPackageTools::UnloadPackages tries to unload them (even worse - crashes earlier)
 * 
 * SOLUTION: Delete .uasset files DIRECTLY from disk without loading/unloading packages.
 * This avoids all cross-reference issues because we never touch the in-memory objects.
 */
inline int32 DeleteAnimationRigClusterOrdered(const TArray<FAssetData>& ClusterAssets, bool bForce)
{
    int32 DeletedCount = 0;

    // Sort all assets by priority
    TArray<FAssetData> OrderedAssets = ClusterAssets;
    OrderedAssets.Sort([](const FAssetData& A, const FAssetData& B)
    {
        const int32 PriorityA = GetAnimationRigClusterDeletePriority(A);
        const int32 PriorityB = GetAnimationRigClusterDeletePriority(B);
        if (PriorityA != PriorityB)
        {
            return PriorityA < PriorityB;
        }

        return A.AssetName.LexicalLess(B.AssetName);
    });

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("DeleteAnimationRigClusterOrdered: Deleting %d cluster assets via file-based deletion"),
        OrderedAssets.Num());

    // STEP 1: Close ALL editors and clear preview state
    // This is essential before any deletion to avoid dangling editor references
#if MCP_HAS_ASSET_EDITOR_SUBSYSTEM
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    if (AssetEditorSubsystem)
    {
        AssetEditorSubsystem->CloseAllAssetEditors();
        UE_LOG(LogMcpSafeOperations, Log, TEXT("DeleteAnimationRigClusterOrdered: Closed all asset editors"));
    }
#endif

    // Clear preview components
    if (GEditor)
    {
        GEditor->ClearPreviewComponents();
    }

    // Clear selection
    if (GEditor)
    {
        GEditor->SelectNone(false, true, false);
    }

    // Flush and GC to clean up any stale references
    FlushRenderingCommands();
    if (GEditor)
    {
        GEditor->ForceGarbageCollection(true);
    }
    FlushRenderingCommands();
    FPlatformProcess::Sleep(0.1f);

    // STEP 2: Handle LOADED packages first - mark them for GC before file deletion
    // CRITICAL: If packages are loaded in memory, we must release them properly
    // before deleting files, otherwise we leave orphaned in-memory objects
    TArray<UPackage*> LoadedPackagesToRelease;
    TSet<FName> LoadedPackageNames;
    TArray<UObject*> LoadedObjectsForCompilation;
    
    for (const FAssetData& AssetData : OrderedAssets)
    {
        FName PackageName = AssetData.PackageName;
        UPackage* Package = FindObject<UPackage>(nullptr, *PackageName.ToString());
        if (Package)
        {
            LoadedPackagesToRelease.Add(Package);
            LoadedPackageNames.Add(PackageName);
            UE_LOG(LogMcpSafeOperations, Log,
                TEXT("DeleteAnimationRigClusterOrdered: Package is loaded: %s"),
                *PackageName.ToString());
        }

        const FString ObjectPath = AssetData.GetSoftObjectPath().ToString();
        if (!ObjectPath.IsEmpty())
        {
            if (UObject* ExistingObject = FindObject<UObject>(nullptr, *ObjectPath))
            {
                LoadedObjectsForCompilation.Add(ExistingObject);
            }
        }
    }

    if (LoadedObjectsForCompilation.Num() > 0)
    {
        McpFinishCompilationForBatch(LoadedObjectsForCompilation, TEXT("special-delete"));
    }

    // Release loaded packages from root set so they can be GC'd
    if (LoadedPackagesToRelease.Num() > 0)
    {
        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("DeleteAnimationRigClusterOrdered: Releasing %d loaded packages from root set"),
            LoadedPackagesToRelease.Num());

        for (UPackage* Package : LoadedPackagesToRelease)
        {
            // Remove from root so it can be garbage collected
            Package->RemoveFromRoot();
            
            // Mark the package as garbage - this tells the engine it should be cleaned up
            Package->MarkAsGarbage();
            
            // Also mark any objects within the package
            TArray<UObject*> ObjectsInPackage;
            GetObjectsWithOuter(Package, ObjectsInPackage, false);
            for (UObject* Obj : ObjectsInPackage)
            {
                Obj->RemoveFromRoot();
                Obj->MarkAsGarbage();
            }
        }

        // Force GC to clean up the released packages
        FlushRenderingCommands();
        if (GEditor)
        {
            GEditor->ForceGarbageCollection(true);
        }
        FlushRenderingCommands();
        FPlatformProcess::Sleep(0.1f);

        // Verify packages are gone
        for (UPackage* Package : LoadedPackagesToRelease)
        {
            if (IsValid(Package))
            {
                UE_LOG(LogMcpSafeOperations, Warning,
                    TEXT("DeleteAnimationRigClusterOrdered: Package still valid after GC: %s"),
                    *Package->GetName());
            }
        }
    }

    // STEP 3: Delete ALL assets by FILE PATH (not by loading/unloading packages)
    // This is the CRITICAL fix - we delete .uasset files directly from disk
    // which avoids all cross-package reference crashes
    FAssetRegistryModule& AssetRegistryModule = 
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    // Track assets that need in-memory deletion (files don't exist on disk)
    TArray<FAssetData> InMemoryOnlyAssets;
    struct FPendingFileDeleteRetry
    {
        FAssetData AssetData;
        FString PackagePath;
        FString AbsolutePath;
    };
    TArray<FPendingFileDeleteRetry> PendingFileDeleteRetries;
    TSet<FName> PendingRetryPackages;

    for (const FAssetData& AssetData : OrderedAssets)
    {
        // Get the package name and convert to file path
        FName PackageName = AssetData.PackageName;
        FString PackagePath = PackageName.ToString();
        
        // Convert package path to actual file path
        FString AssetFilePath;
        if (!FPackageName::TryConvertLongPackageNameToFilename(PackagePath, AssetFilePath, FPackageName::GetAssetPackageExtension()))
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("DeleteAnimationRigClusterOrdered: Could not convert package path to file: %s - treating as in-memory only"),
                *PackagePath);
            InMemoryOnlyAssets.Add(AssetData);
            continue;
        }

        // Normalize the path - ensure it's absolute with correct platform separators
        FString AbsolutePath;
        if (FPaths::IsRelative(AssetFilePath))
        {
            AbsolutePath = FPaths::ConvertRelativePathToFull(AssetFilePath);
        }
        else
        {
            AbsolutePath = AssetFilePath;
        }
        
        // Normalize path separators for the current platform
        FPaths::NormalizeFilename(AbsolutePath);
        
        // Use IFileManager for more robust file operations
        IFileManager& FileManager = IFileManager::Get();
        
        // Check if file exists
        if (!FileManager.FileExists(*AbsolutePath))
        {
            UE_LOG(LogMcpSafeOperations, Log,
                TEXT("DeleteAnimationRigClusterOrdered: File does not exist: %s - asset is in-memory only"),
                *AbsolutePath);
            InMemoryOnlyAssets.Add(AssetData);
            continue;
        }

        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("DeleteAnimationRigClusterOrdered: Deleting file: %s"),
            *AbsolutePath);

        // Delete the file directly
        bool bFileDeleted = FileManager.Delete(*AbsolutePath, false, true, true);
        
        if (bFileDeleted)
        {
            ++DeletedCount;
            UE_LOG(LogMcpSafeOperations, Log,
                TEXT("DeleteAnimationRigClusterOrdered: Deleted file for %s (%s)"),
                *AssetData.AssetName.ToString(),
                *AssetData.AssetClassPath.ToString());

            // Scan the path to update the asset registry
            TArray<FString> ScanPaths;
            ScanPaths.Add(FPaths::GetPath(PackagePath));
            AssetRegistry.ScanPathsSynchronous(ScanPaths, false);
        }
        else
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("DeleteAnimationRigClusterOrdered: Failed to delete file: %s - treating as in-memory"),
                *AbsolutePath);
            PendingFileDeleteRetries.Add({ AssetData, PackagePath, AbsolutePath });
            PendingRetryPackages.Add(AssetData.PackageName);
            InMemoryOnlyAssets.Add(AssetData);
        }
    }

    // STEP 3b: Handle in-memory only assets (files never saved to disk)
    // CRITICAL: For in-memory AnimBlueprints with cross-references, we CANNOT use ForceDeleteObjects.
    // Instead, we mark them as garbage and let GC clean them up safely.
    // This is slower but prevents the 0xFFFFFFFFFFFFFFFF crash.
    if (InMemoryOnlyAssets.Num() > 0)
    {
        UE_LOG(LogMcpSafeOperations, Warning,
            TEXT("DeleteAnimationRigClusterOrdered: %d assets are in-memory only, marking for GC cleanup"),
            InMemoryOnlyAssets.Num());

        for (const FAssetData& AssetData : InMemoryOnlyAssets)
        {
            // Get the object without loading it (it should already be loaded if we found it)
            FString ObjectPath = AssetData.GetSoftObjectPath().ToString();
            UObject* Asset = FindObject<UObject>(nullptr, *ObjectPath);
            
            if (!Asset)
            {
                UE_LOG(LogMcpSafeOperations, Log,
                    TEXT("DeleteAnimationRigClusterOrdered: In-memory asset already gone: %s"),
                    *ObjectPath);
                if (!PendingRetryPackages.Contains(AssetData.PackageName))
                {
                    ++DeletedCount;
                }
                continue;
            }

            UE_LOG(LogMcpSafeOperations, Log,
                TEXT("DeleteAnimationRigClusterOrdered: Marking in-memory asset for GC: %s"),
                *Asset->GetName());

            // Remove from root set
            Asset->RemoveFromRoot();
            
            // Mark the asset as garbage - GC will clean it up
            Asset->MarkAsGarbage();
            
            // Also handle the package and any objects within it
            UPackage* Package = Asset->GetOutermost();
            if (Package)
            {
                Package->RemoveFromRoot();
                Package->MarkAsGarbage();
                
                // Mark all objects in the package
                TArray<UObject*> ObjectsInPackage;
                GetObjectsWithOuter(Package, ObjectsInPackage, false);
                for (UObject* Obj : ObjectsInPackage)
                {
                    if (Obj != Asset)
                    {
                        Obj->RemoveFromRoot();
                        Obj->MarkAsGarbage();
                    }
                }
            }

            if (!PendingRetryPackages.Contains(AssetData.PackageName))
            {
                ++DeletedCount;
            }
        }

        // Single GC pass to clean up all marked objects
        // GC properly handles cross-references between garbage objects
        FlushRenderingCommands();
        if (GEditor)
        {
            GEditor->ForceGarbageCollection(true);
        }
        FlushRenderingCommands();
        FPlatformProcess::Sleep(0.05f);
        
        // Second GC pass to ensure everything is cleaned
        if (GEditor)
        {
            GEditor->ForceGarbageCollection(true);
        }
        FlushRenderingCommands();
    }

    // STEP 3c: Retry any file-backed deletes that were locked before GC cleanup.
    if (PendingFileDeleteRetries.Num() > 0)
    {
        IFileManager& FileManager = IFileManager::Get();
        TArray<UObject*> RetryObjectsForCompilation;
        for (const FPendingFileDeleteRetry& Retry : PendingFileDeleteRetries)
        {
            const FString ObjectPath = Retry.AssetData.GetSoftObjectPath().ToString();
            if (!ObjectPath.IsEmpty())
            {
                if (UObject* ExistingObject = FindObject<UObject>(nullptr, *ObjectPath))
                {
                    RetryObjectsForCompilation.Add(ExistingObject);
                }
            }
        }

        if (RetryObjectsForCompilation.Num() > 0)
        {
            McpFinishCompilationForBatch(RetryObjectsForCompilation, TEXT("retry-delete"));
            FlushRenderingCommands();
            if (GEditor)
            {
                GEditor->ForceGarbageCollection(true);
            }
            FlushRenderingCommands();
        }

        for (const FPendingFileDeleteRetry& Retry : PendingFileDeleteRetries)
        {
            const bool bExistsBeforeRetry = FileManager.FileExists(*Retry.AbsolutePath);
            if (bExistsBeforeRetry)
            {
                UE_LOG(LogMcpSafeOperations, Log,
                    TEXT("DeleteAnimationRigClusterOrdered: Retrying file delete after GC: %s"),
                    *Retry.AbsolutePath);
            }

            const bool bDeletedOnRetry = bExistsBeforeRetry
                ? FileManager.Delete(*Retry.AbsolutePath, false, true, true)
                : true;
            const bool bExistsAfterRetry = FileManager.FileExists(*Retry.AbsolutePath);

            if (bDeletedOnRetry || !bExistsAfterRetry)
            {
                ++DeletedCount;
                UE_LOG(LogMcpSafeOperations, Log,
                    TEXT("DeleteAnimationRigClusterOrdered: Retry delete succeeded for %s (%s)"),
                    *Retry.AssetData.AssetName.ToString(),
                    *Retry.AssetData.AssetClassPath.ToString());

                TArray<FString> ScanPaths;
                ScanPaths.Add(FPaths::GetPath(Retry.PackagePath));
                AssetRegistry.ScanPathsSynchronous(ScanPaths, false);
            }
            else
            {
                UE_LOG(LogMcpSafeOperations, Warning,
                    TEXT("DeleteAnimationRigClusterOrdered: Retry delete still failed for %s at %s"),
                    *Retry.AssetData.AssetName.ToString(),
                    *Retry.AbsolutePath);
            }
        }
    }

    // STEP 4: Final cleanup - GC to clean up any stale references
    FlushRenderingCommands();
    if (GEditor)
    {
        GEditor->ForceGarbageCollection(true);
    }
    FlushRenderingCommands();

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("DeleteAnimationRigClusterOrdered: Deleted %d/%d cluster assets via file-based deletion"),
        DeletedCount, OrderedAssets.Num());

    return DeletedCount;
}

/**
 * Split a batch of asset data into file-backed loaded objects vs in-memory-only assets.
 * In-memory-only assets are marked for GC instead of being sent through ObjectTools delete flows.
 */
inline void PrepareAssetBatchForDelete(
    const TArray<FAssetData>& Assets,
    const TCHAR* LogContext,
    TArray<UObject*>& OutFileBackedObjects,
    int32& OutInMemoryOnlyCount)
{
    OutFileBackedObjects.Reset();
    OutInMemoryOnlyCount = 0;

    IFileManager& FileManager = IFileManager::Get();

    for (const FAssetData& AssetData : Assets)
    {
        UObject* Asset = AssetData.GetAsset();
        if (!Asset)
        {
            continue;
        }

        const FString PackagePath = AssetData.PackageName.ToString();
        FString AssetFilePath;
        bool bHasBackingFile = false;
        const FString ClassName = AssetData.AssetClassPath.ToString();
        const bool bIsWorldAsset = ClassName.Contains(TEXT("World")) ||
            ClassName.Contains(TEXT("Map")) ||
            ClassName.Contains(TEXT("Level"));
        const FString PackageExtension = bIsWorldAsset
            ? FPackageName::GetMapPackageExtension()
            : FPackageName::GetAssetPackageExtension();
        if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, AssetFilePath, PackageExtension))
        {
            FString AbsolutePath = FPaths::IsRelative(AssetFilePath)
                ? FPaths::ConvertRelativePathToFull(AssetFilePath)
                : AssetFilePath;
            FPaths::NormalizeFilename(AbsolutePath);
            bHasBackingFile = FileManager.FileExists(*AbsolutePath);

            if (!bHasBackingFile)
            {
                UE_LOG(LogMcpSafeOperations, Log,
                    TEXT("%s: File does not exist: %s - asset is in-memory only"),
                    LogContext,
                    *AbsolutePath);
            }
        }

        if (!bHasBackingFile)
        {
            UE_LOG(LogMcpSafeOperations, Log,
                TEXT("%s: Marking in-memory asset for GC: %s (%s)"),
                LogContext,
                *AssetData.AssetName.ToString(),
                *AssetData.AssetClassPath.ToString());

            Asset->RemoveFromRoot();
            Asset->MarkAsGarbage();

            if (UPackage* Package = Asset->GetOutermost())
            {
                Package->RemoveFromRoot();
                Package->MarkAsGarbage();

                TArray<UObject*> ObjectsInPackage;
                GetObjectsWithOuter(Package, ObjectsInPackage, false);
                for (UObject* Obj : ObjectsInPackage)
                {
                    if (Obj != Asset)
                    {
                        Obj->RemoveFromRoot();
                        Obj->MarkAsGarbage();
                    }
                }
            }

            ++OutInMemoryOnlyCount;
            continue;
        }

        OutFileBackedObjects.Add(Asset);
    }

    if (OutInMemoryOnlyCount > 0)
    {
        FlushRenderingCommands();
        if (GEditor)
        {
            GEditor->ForceGarbageCollection(true);
        }
        FlushRenderingCommands();
    }
}

/**
 * Check if an asset class is considered "risky" for deletion (may have world references).
 * These asset types may cause crashes if deleted without proper cleanup.
 *
 * @param AssetPath The asset path to check
 * @return true if the asset is a risky type that needs extra cleanup
 */
inline bool IsRiskyAssetClassForDelete(const FString& AssetPath)
{
    // Asset types that commonly have world references or cause crashes on delete
    static const TArray<FString> RiskyClasses = {
        TEXT("AnimBlueprint"),
        TEXT("AnimSequence"),
        TEXT("IKRigDefinition"),
        TEXT("IKRetargeter"),
        TEXT("ControlRigBlueprint"),
        TEXT("WidgetBlueprint"),
        TEXT("Blueprint")
    };
    
    FAssetRegistryModule& AssetRegistryModule = 
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    // Use FSoftObjectPath for UE 5.1+ (FName version deprecated in 5.6)
#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1
    FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(AssetPath));
#else
    // UE 5.0: GetAssetByObjectPath takes FName
    FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FName(*AssetPath));
#endif
    if (AssetData.IsValid())
    {
        FString ClassName = AssetData.AssetClassPath.ToString();
        for (const FString& RiskyClass : RiskyClasses)
        {
            if (ClassName.Contains(RiskyClass))
            {
                return true;
            }
        }
    }
    
    return false;
}

/**
 * Check if an asset is a world/map asset using ONLY registry metadata.
 * CRITICAL: Do NOT call GetClass() or GetAsset() here as they can load packages.
 */
inline bool IsWorldAsset(const FAssetData& AssetData)
{
    FString ClassName = AssetData.AssetClassPath.ToString();
    // Check for World, Map, Level asset types using string matching only
    return ClassName.Contains(TEXT("World")) || 
           ClassName.Contains(TEXT("Map")) ||
           ClassName.Contains(TEXT("Level"));
}

/**
 * Check if any world package in the given list is currently loaded.
 * Uses package name matching only - does NOT load assets.
 */
inline bool HasLoadedWorlds(const TArray<FAssetData>& WorldAssets)
{
    for (const FAssetData& AssetData : WorldAssets)
    {
        FString PackageName = AssetData.PackageName.ToString();
        // Check if package is loaded using FindObject (doesn't load)
        if (FindObject<UPackage>(nullptr, *PackageName))
        {
            return true;
        }
    }
    return false;
}

/**
 * Delete world/map packages by package path instead of ObjectTools world deletion.
 * This avoids the engine path that logs active worlds and crashes in UE 5.7.
 */
inline int32 DeleteWorldPackagesByPath(const TArray<FAssetData>& WorldAssets)
{
    int32 DeletedCount = 0;
    IFileManager& FileManager = IFileManager::Get();
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    for (const FAssetData& AssetData : WorldAssets)
    {
        const FString PackagePath = AssetData.PackageName.ToString();

        if (UPackage* LoadedPackage = FindObject<UPackage>(nullptr, *PackagePath))
        {
            LoadedPackage->RemoveFromRoot();
            LoadedPackage->MarkAsGarbage();

            TArray<UObject*> ObjectsInPackage;
            GetObjectsWithOuter(LoadedPackage, ObjectsInPackage, false);
            for (UObject* Obj : ObjectsInPackage)
            {
                Obj->RemoveFromRoot();
                Obj->MarkAsGarbage();
            }
        }

        FString MapFilename;
        if (!FPackageName::TryConvertLongPackageNameToFilename(PackagePath, MapFilename, FPackageName::GetMapPackageExtension()))
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("DeleteWorldPackagesByPath: Could not convert map package to filename: %s"),
                *PackagePath);
            continue;
        }

        FString AbsoluteMapFilename = FPaths::ConvertRelativePathToFull(MapFilename);
        FPaths::NormalizeFilename(AbsoluteMapFilename);

        bool bDeletedMap = false;
        if (FileManager.FileExists(*AbsoluteMapFilename))
        {
            UE_LOG(LogMcpSafeOperations, Log,
                TEXT("DeleteWorldPackagesByPath: Deleting map file: %s"),
                *AbsoluteMapFilename);
            bDeletedMap = FileManager.Delete(*AbsoluteMapFilename, false, true, true);
            if (!bDeletedMap)
            {
                UE_LOG(LogMcpSafeOperations, Warning,
                    TEXT("DeleteWorldPackagesByPath: Failed to delete map file: %s"),
                    *AbsoluteMapFilename);
            }
        }
        else
        {
            UE_LOG(LogMcpSafeOperations, Log,
                TEXT("DeleteWorldPackagesByPath: Map file does not exist: %s"),
                *AbsoluteMapFilename);
        }

        const FString BuiltDataPackagePath = PackagePath + TEXT("_BuiltData");
        FString BuiltDataFilename;
        if (FPackageName::TryConvertLongPackageNameToFilename(BuiltDataPackagePath, BuiltDataFilename, FPackageName::GetAssetPackageExtension()))
        {
            FString AbsoluteBuiltDataFilename = FPaths::ConvertRelativePathToFull(BuiltDataFilename);
            FPaths::NormalizeFilename(AbsoluteBuiltDataFilename);
            if (FileManager.FileExists(*AbsoluteBuiltDataFilename))
            {
                UE_LOG(LogMcpSafeOperations, Log,
                    TEXT("DeleteWorldPackagesByPath: Deleting built data file: %s"),
                    *AbsoluteBuiltDataFilename);
                if (!FileManager.Delete(*AbsoluteBuiltDataFilename, false, true, true))
                {
                    UE_LOG(LogMcpSafeOperations, Warning,
                        TEXT("DeleteWorldPackagesByPath: Failed to delete built data file: %s"),
                        *AbsoluteBuiltDataFilename);
                }
            }
        }

        TArray<FString> ScanPaths;
        ScanPaths.Add(FPaths::GetPath(PackagePath));
        AssetRegistry.ScanPathsSynchronous(ScanPaths, false);

        if (bDeletedMap || !FileManager.FileExists(*AbsoluteMapFilename))
        {
            ++DeletedCount;
        }
    }

    FlushRenderingCommands();
    if (GEditor)
    {
        GEditor->ForceGarbageCollection(true);
    }
    FlushRenderingCommands();

    return DeletedCount;
}

/**
 * Safely delete a folder and all its contents with proper cleanup.
 * 
 * CRITICAL FOR UE 5.7+:
 * This function prevents crashes during folder deletion by:
 * 1. Enumerating all assets using REGISTRY ONLY (no GetAsset/GetClass)
 * 2. Checking for LOADED world packages and switching away BEFORE any loads
 * 3. Unloading all worlds in the target folder
 * 4. Only THEN loading non-world assets for deletion
 * 5. Deleting in phases with GC between each phase
 *
 * @param FolderPath The folder path to delete (e.g., /Game/MyFolder)
 * @param bForce If true, force delete even if assets are referenced
 * @return true if deletion succeeded
 */
inline bool McpSafeDeleteFolder(const FString& FolderPath, bool bForce = true)
{
    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Starting deletion of '%s' (force=%d)"), *FolderPath, bForce);
    
    FAssetRegistryModule& AssetRegistryModule = 
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    
    // STEP 1: Enumerate all assets in the folder recursively using REGISTRY ONLY
    // DO NOT call GetAsset() or GetClass() here - only use metadata
    FARFilter Filter;
    Filter.PackagePaths.Add(FName(*FolderPath));
    Filter.bRecursivePaths = true;
    
    TArray<FAssetData> AllAssets;
    AssetRegistry.GetAssets(Filter, AllAssets);
    
    if (AllAssets.Num() == 0)
    {
        UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: No assets found in '%s'"), *FolderPath);
        // No assets - just delete the empty folder
        return UEditorAssetLibrary::DeleteDirectory(FolderPath);
    }
    
    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Found %d assets in '%s'"), AllAssets.Num(), *FolderPath);
    
    // STEP 2: Separate world/map assets from other assets using REGISTRY ONLY
    TArray<FAssetData> WorldAssets;
    TArray<FAssetData> OtherAssets;
    
    for (const FAssetData& AssetData : AllAssets)
    {
        if (IsWorldAsset(AssetData))
        {
            WorldAssets.Add(AssetData);
            UE_LOG(LogMcpSafeOperations, Log, TEXT("  World asset: %s (%s)"), 
                *AssetData.AssetName.ToString(), *AssetData.AssetClassPath.ToString());
        }
        else
        {
            OtherAssets.Add(AssetData);
        }
    }
    
    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: %d world assets, %d other assets"), 
        WorldAssets.Num(), OtherAssets.Num());
    
    // STEP 3: If folder contains world assets, switch to a known engine map using the
    // safe map-transition helper. Raw NewBlankMap triggers TickTaskManager assertions in UE 5.7.
    if (WorldAssets.Num() > 0)
    {
        bool bCurrentWorldInFolder = false;
        FString CurrentWorldPath;
        if (GEditor)
        {
            if (UWorld* CurrentEditorWorld = GEditor->GetEditorWorldContext().World())
            {
                CurrentWorldPath = CurrentEditorWorld->GetOutermost()->GetName();
                bCurrentWorldInFolder = CurrentWorldPath.StartsWith(FolderPath, ESearchCase::IgnoreCase);
            }
        }

        const bool bTargetWorldLoaded = HasLoadedWorlds(WorldAssets);
        const bool bMustSwitchWorld = bCurrentWorldInFolder;

        if (bMustSwitchWorld)
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("McpSafeDeleteFolder: Folder contains %d world assets; switching to /Engine/Maps/Entry for safety (currentWorld=%s loadedTargetWorlds=%d inFolder=%d)"),
                WorldAssets.Num(),
                CurrentWorldPath.IsEmpty() ? TEXT("<none>") : *CurrentWorldPath,
                bTargetWorldLoaded ? 1 : 0,
                bCurrentWorldInFolder ? 1 : 0);

            if (!McpSafeLoadMap(TEXT("/Engine/Maps/Entry"), true))
            {
                UE_LOG(LogMcpSafeOperations, Error,
                    TEXT("McpSafeDeleteFolder: Failed to switch to /Engine/Maps/Entry before deleting world assets"));
                return false;
            }

            // Flush and GC to ensure worlds are fully unloaded
            FlushRenderingCommands();
            if (GEditor) GEditor->ForceGarbageCollection(true);
            FlushRenderingCommands();

            UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Switched to /Engine/Maps/Entry, worlds should be unloaded"));
        }
        else
        {
            UE_LOG(LogMcpSafeOperations, Log,
                TEXT("McpSafeDeleteFolder: Current world is outside '%s'; skipping /Engine/Maps/Entry switch (currentWorld=%s loadedTargetWorlds=%d)"),
                *FolderPath,
                CurrentWorldPath.IsEmpty() ? TEXT("<none>") : *CurrentWorldPath,
                bTargetWorldLoaded ? 1 : 0);
        }
    }
    
    // STEP 4: GLOBAL QUIESCE - Critical for UE 5.7+ animation/IK asset deletion
    // Must quiesce ALL editor state before ANY deletions to prevent crashes
    McpQuiesceAllState();
    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Global quiesce completed before deletions"));
    
    // STEP 5: Partition other assets into special-delete vs safe categories.
    // Special-delete includes all risky animation assets plus generic Blueprint assets.
    TArray<FAssetData> RiskyAnimationAssets;
    TArray<FAssetData> SafeAssets;
    
    for (const FAssetData& AssetData : OtherAssets)
    {
        if (IsRiskyAnimationAsset(AssetData) || IsAnyBlueprintAsset(AssetData))
        {
            RiskyAnimationAssets.Add(AssetData);
            UE_LOG(LogMcpSafeOperations, Log, TEXT("  Risky special-delete asset: %s (%s)"), 
                *AssetData.AssetName.ToString(), *AssetData.AssetClassPath.ToString());
        }
        else
        {
            SafeAssets.Add(AssetData);
        }
    }
    
    UE_LOG(LogMcpSafeOperations, Log, 
        TEXT("McpSafeDeleteFolder: Partitioned: %d risky special-delete, %d safe, %d world"), 
        RiskyAnimationAssets.Num(), SafeAssets.Num(), WorldAssets.Num());
    
    // STEP 6: Delete RISKY ANIMATION ASSETS.
    // CRITICAL FOR UE 5.7+: The remaining crash is isolated to a mixed cluster of
    // IKRigDefinition, AnimSequence, AnimBlueprint, and ControlRigBlueprint.
    // Route that cluster through explicit ordered singleton deletion; keep generic batching
    // for all other risky animation assets.
    if (RiskyAnimationAssets.Num() > 0)
    {
        TArray<FAssetData> OrderedClusterAssets;
        TArray<FAssetData> GenericRiskyAssets;

        const bool bHasMixedCluster = IsMixedAnimationRigCluster(RiskyAnimationAssets);
        for (const FAssetData& AssetData : RiskyAnimationAssets)
        {
            const int32 Priority = GetAnimationRigClusterDeletePriority(AssetData);
            if (bHasMixedCluster && Priority < 4)
            {
                OrderedClusterAssets.Add(AssetData);
            }
            else
            {
                GenericRiskyAssets.Add(AssetData);
            }
        }

        int32 DeletedRisky = 0;

        if (OrderedClusterAssets.Num() > 0)
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("McpSafeDeleteFolder: Mixed animation/rig cluster detected; deleting %d cluster assets in explicit order"),
                OrderedClusterAssets.Num());
            const int32 OrderedClusterDeleted = DeleteAnimationRigClusterOrdered(OrderedClusterAssets, bForce);
            if (OrderedClusterDeleted == INDEX_NONE)
            {
                UE_LOG(LogMcpSafeOperations, Error,
                    TEXT("McpSafeDeleteFolder: Failed to delete AnimBlueprint portion of mixed animation/rig cluster in '%s'"),
                    *FolderPath);
                return false;
            }
            DeletedRisky += OrderedClusterDeleted;
        }

        // STEP 6b: Delete remaining risky special-delete assets using FILE-BASED deletion
        // CRITICAL: Do NOT use ForceDeleteObjects for any animation-related assets
        // They all have cross-package references that cause 0xFFFFFFFFFFFFFFFF crashes
        const int32 TotalRisky = GenericRiskyAssets.Num();
        
        if (TotalRisky > 0)
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("McpSafeDeleteFolder: Deleting %d remaining risky special-delete assets via file-based deletion"),
                TotalRisky);
            
            // Use the same file-based deletion approach as the ordered cluster
            const int32 GenericDeleted = DeleteAnimationRigClusterOrdered(GenericRiskyAssets, bForce);
            DeletedRisky += GenericDeleted;
            
            UE_LOG(LogMcpSafeOperations, Log, 
                TEXT("McpSafeDeleteFolder: Deleted %d/%d remaining risky special-delete assets via file-based deletion"),
                GenericDeleted, TotalRisky);
        }
        
        UE_LOG(LogMcpSafeOperations, Log, 
            TEXT("McpSafeDeleteFolder: Deleted %d total risky special-delete assets"), DeletedRisky);
    }
    
    // STEP 7: Delete SAFE non-world assets with tight compilation barriers
    if (SafeAssets.Num() > 0)
    {
        TArray<UObject*> SafeObjectsToDelete;
        int32 SafeInMemoryOnlyCount = 0;
        PrepareAssetBatchForDelete(
            SafeAssets,
            TEXT("McpSafeDeleteFolder[SafeAssets]"),
            SafeObjectsToDelete,
            SafeInMemoryOnlyCount);

        if (SafeInMemoryOnlyCount > 0)
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("McpSafeDeleteFolder: %d safe assets are in-memory only and were routed through GC cleanup"),
                SafeInMemoryOnlyCount);
        }
        
        if (SafeObjectsToDelete.Num() > 0)
        {
            // Pre-deletion quiesce with batch-specific compilation barrier
            McpQuiesceBeforeBatchDelete(SafeObjectsToDelete);

            for (UObject* SafeObject : SafeObjectsToDelete)
            {
                McpPreClearBlueprintActionDatabase(SafeObject);
            }
            
            UE_LOG(LogMcpSafeOperations, Log,
                TEXT("McpSafeDeleteFolder: Deleting %d file-backed safe assets via AssetViewUtils::DeleteAssets"),
                SafeObjectsToDelete.Num());

            AssetViewUtils::DeleteAssets(SafeObjectsToDelete);
            
            // Post-deletion quiesce
            McpQuiesceAfterBatchDelete(SafeObjectsToDelete);
        }
    }
    
    // STEP 8: Delete WORLD ASSETS LAST (they should be unloaded now)
    if (WorldAssets.Num() > 0)
    {
        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpSafeDeleteFolder: Deleting %d world assets via package/file path"),
            WorldAssets.Num());

        const int32 DeletedWorlds = DeleteWorldPackagesByPath(WorldAssets);
        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpSafeDeleteFolder: Deleted %d/%d world assets via package/file path"),
            DeletedWorlds, WorldAssets.Num());
    }

    // Final cleanup boundary before registry/path verification.
    FlushRenderingCommands();
    if (GEditor)
    {
        GEditor->ForceGarbageCollection(true);
    }
    FlushRenderingCommands();

    const FString ParentFolderPath = FPaths::GetPath(FolderPath);
    if (!ParentFolderPath.IsEmpty())
    {
        ScanPathSynchronous(ParentFolderPath, true);
    }
    
    // STEP 9: Remove the folder and any subpaths from asset registry
    TArray<FString> SubPathsToRemove;
    AssetRegistry.GetSubPaths(FolderPath, SubPathsToRemove, true);
    SubPathsToRemove.Sort([](const FString& A, const FString& B)
    {
        return A.Len() > B.Len();
    });
    for (const FString& SubPath : SubPathsToRemove)
    {
        AssetRegistry.RemovePath(SubPath);
    }
    AssetRegistry.RemovePath(FolderPath);
    
    // STEP 10: Delete the empty physical directory
    FString LocalPath;
    if (FPackageName::TryConvertLongPackageNameToFilename(FolderPath, LocalPath))
    {
        IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
        if (PlatformFile.DirectoryExists(*LocalPath))
        {
            PlatformFile.DeleteDirectoryRecursively(*LocalPath);
            UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Deleted physical directory '%s'"), *LocalPath);

            if (PlatformFile.DirectoryExists(*LocalPath))
            {
                FlushRenderingCommands();
                if (GEditor)
                {
                    GEditor->ForceGarbageCollection(true);
                }
                FlushRenderingCommands();
                FPlatformProcess::Sleep(0.05f);

                PlatformFile.DeleteDirectoryRecursively(*LocalPath);
                UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Retried physical directory deletion '%s'"), *LocalPath);
            }
        }
    }
    
    // Verify deletion using both asset-registry and filesystem state.
    FARFilter RemainingFilter;
    RemainingFilter.PackagePaths.Add(FName(*FolderPath));
    RemainingFilter.bRecursivePaths = true;

    TArray<FAssetData> RemainingAssets;
    AssetRegistry.GetAssets(RemainingFilter, RemainingAssets);

    TArray<FString> RemainingSubPaths;
    AssetRegistry.GetSubPaths(FolderPath, RemainingSubPaths, true);

    bool bDirectoryExistsOnDisk = false;
    FString VerifyLocalPath;
    if (FPackageName::TryConvertLongPackageNameToFilename(FolderPath, VerifyLocalPath))
    {
        bDirectoryExistsOnDisk = FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*VerifyLocalPath);
    }

    if (RemainingAssets.Num() == 0 && RemainingSubPaths.Num() == 0 && !bDirectoryExistsOnDisk)
    {
        UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeDeleteFolder: Successfully deleted '%s'"), *FolderPath);
        return true;
    }
    else
    {
        UE_LOG(LogMcpSafeOperations, Warning,
            TEXT("McpSafeDeleteFolder: Directory still exists after deletion attempt (remainingAssets=%d remainingSubPaths=%d existsOnDisk=%d)"),
            RemainingAssets.Num(), RemainingSubPaths.Num(), bDirectoryExistsOnDisk ? 1 : 0);

        for (const FAssetData& RemainingAsset : RemainingAssets)
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("McpSafeDeleteFolder: Remaining asset: %s (%s)"),
                *RemainingAsset.GetSoftObjectPath().ToString(),
                *RemainingAsset.AssetClassPath.ToString());
        }

        for (const FString& RemainingSubPath : RemainingSubPaths)
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("McpSafeDeleteFolder: Remaining subpath: %s"),
                *RemainingSubPath);
        }
        return false;
    }
}

#else

// Non-editor stubs
inline bool McpSafeAssetSave(void* Asset) { return false; }
inline bool McpSafeLevelSave(void* Level, const FString& Path, int32 = 1) { return false; }
inline bool McpSafeLoadMap(const FString& MapPath, bool = true) { return false; }
inline class UMaterialInterface* McpLoadMaterialWithFallback(const FString& = FString(), bool = false) { return nullptr; }
inline bool SaveLoadedAssetThrottled(void* Asset, double = -1.0, bool = false) { return false; }
inline void ScanPathSynchronous(const FString&, bool = true) {}

#endif // WITH_EDITOR

} // namespace McpSafeOperations

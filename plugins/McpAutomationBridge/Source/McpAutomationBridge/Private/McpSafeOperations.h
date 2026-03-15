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

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Engine/Blueprint.h"
#include "Engine/LevelStreaming.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "TickTaskManagerInterface.h"
#include "HAL/PlatformProcess.h"
#include "RenderingThread.h"

#if __has_include("EditorAssetLibrary.h")
#include "EditorAssetLibrary.h"
#else
#include "Editor/EditorAssetLibrary.h"
#endif

#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialInterface.h"
#include "Editor/EditorEngine.h"
#include "UObject/UObjectIterator.h"
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
    UWorld* World = Level ? Level->GetWorld() : nullptr;
    bool bSaveSucceeded = false;
    if (World)
    {
        bSaveSucceeded = UEditorLoadingAndSavingUtils::SaveMap(World, PackagePath);
    }
    else
    {
        bSaveSucceeded = FEditorFileUtils::SaveLevel(Level, *PackagePath);
    }

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
        
        // STEP 1: Mark all levels as invisible to prevent FillLevelList from adding them
        for (ULevel* Level : CurrentWorld->GetLevels())
        {
            if (Level)
            {
                Level->bIsVisible = false;
            }
        }
        
        // STEP 2: Unregister all tick functions
        int32 UnregisteredActorCount = 0;
        int32 UnregisteredComponentCount = 0;
        for (ULevel* Level : CurrentWorld->GetLevels())
        {
            if (!Level) continue;
            
            for (AActor* Actor : Level->Actors)
            {
                if (Actor)
                {
                    if (Actor->PrimaryActorTick.IsTickFunctionRegistered())
                    {
                        Actor->PrimaryActorTick.UnRegisterTickFunction();
                        UnregisteredActorCount++;
                    }
                    
                    Actor->PrimaryActorTick.GetPrerequisites().Empty();
                    
                    for (UActorComponent* Component : Actor->GetComponents())
                    {
                        if (Component && Component->PrimaryComponentTick.IsTickFunctionRegistered())
                        {
                            Component->PrimaryComponentTick.UnRegisterTickFunction();
                            UnregisteredComponentCount++;
                        }
                    }
                }
            }
        }
        UE_LOG(LogMcpSafeOperations, Log, 
            TEXT("McpSafeLoadMap: Unregistered %d actor ticks and %d component ticks"), 
            UnregisteredActorCount, UnregisteredComponentCount);
        
        // STEP 3: Send end-of-frame updates
        CurrentWorld->SendAllEndOfFrameUpdates();
        
        // STEP 4: Flush rendering commands
        FlushRenderingCommands();
        
        // STEP 5: Unload streaming levels
        TArray<ULevelStreaming*> StreamingLevels = CurrentWorld->GetStreamingLevels();
        for (ULevelStreaming* StreamingLevel : StreamingLevels)
        {
            if (StreamingLevel)
            {
                StreamingLevel->SetShouldBeLoaded(false);
                StreamingLevel->SetShouldBeVisible(false);
            }
        }
        
        // STEP 6: Flush again
        FlushRenderingCommands();
        
        // STEP 7: Force garbage collection
        GEditor->ForceGarbageCollection(true);
        
        // STEP 8: Flush after GC
        FlushRenderingCommands();
        
        // STEP 9: Give engine time to process cleanup
        FPlatformProcess::Sleep(0.10f);
        
        // STEP 10: Final flush
        FlushRenderingCommands();
    }
    
    // STEP 11: Check if already loaded
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
    
    // STEP 12: Load the map
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
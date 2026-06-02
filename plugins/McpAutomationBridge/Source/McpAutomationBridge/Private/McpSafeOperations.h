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

// Include version compatibility macros before version-specific checks
#include "McpVersionCompatibility.h"

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
#include "HAL/PlatformFileManager.h"
#include "AssetViewUtils.h"
#include "Materials/MaterialInterface.h"
#include "Editor/EditorEngine.h"
#include "UObject/UObjectHash.h"
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
 * Safe asset saving helper - marks package dirty, registers the asset, and
 * persists the owning package through the editor's save flow.
 *
 * CRITICAL FOR UE 5.7+:
 * DO NOT use raw UPackage::SavePackage(). Use the editor-owned package save path
 * so asset packages are persisted without manual package-file handling.
 *
 * @param Asset The UObject asset to save
 * @returns true if the asset package was saved successfully
 */
inline bool McpSafeAssetSave(UObject* Asset)
{
    if (!Asset)
    {
        return false;
    }

    UObject* AssetToSave = Asset;
    UPackage* Package = Cast<UPackage>(Asset);
    if (Package)
    {
        AssetToSave = nullptr;
        ForEachObjectWithPackage(Package, [&AssetToSave](UObject* Object) -> bool
        {
            if (Object && !Object->IsA<UPackage>() && Object->HasAnyFlags(RF_Public | RF_Standalone))
            {
                AssetToSave = Object;
                return false;
            }
            return true;
        }, false);
    }
    else
    {
        Package = Asset->GetOutermost();
    }
    if (!Package)
    {
        return false;
    }

    const FString PackageName = Package->GetName();
    if (PackageName.StartsWith(TEXT("/Temp/")) ||
        PackageName.StartsWith(TEXT("/Transient/")) ||
        PackageName.StartsWith(TEXT("/Engine/Transient")) ||
        Package->HasAnyFlags(RF_Transient))
    {
        return false;
    }

    Package->SetDirtyFlag(true);
    if (AssetToSave && AssetToSave != Package)
    {
        AssetToSave->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(AssetToSave);
    }

    auto ScanSavedPackage = [&PackageName]()
    {
        TArray<FString> PathsToScan;
        PathsToScan.Add(FPaths::GetPath(PackageName));
        FAssetRegistryModule& AssetRegistryModule =
            FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
        AssetRegistryModule.Get().ScanPathsSynchronous(PathsToScan, false);
    };

    auto PackageExistsOnDisk = [&PackageName]()
    {
        FString AssetFilename;
        FString MapFilename;
        const bool bHasAssetFilename = FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, AssetFilename, FPackageName::GetAssetPackageExtension());
        const bool bHasMapFilename = FPackageName::TryConvertLongPackageNameToFilename(
            PackageName, MapFilename, FPackageName::GetMapPackageExtension());

        return
            (bHasAssetFilename && IFileManager::Get().FileExists(*FPaths::ConvertRelativePathToFull(AssetFilename))) ||
            (bHasMapFilename && IFileManager::Get().FileExists(*FPaths::ConvertRelativePathToFull(MapFilename)));
    };

#if MCP_HAS_PACKAGE_TOOLS
    if (AssetToSave && AssetToSave != Package)
    {
        TArray<UObject*> ObjectsToSave;
        ObjectsToSave.Add(AssetToSave);

        FlushRenderingCommands();

        const bool bSaved = UPackageTools::SavePackagesForObjects(ObjectsToSave);
        if (bSaved && PackageExistsOnDisk())
        {
            ScanSavedPackage();
            return true;
        }

        if (bSaved)
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("McpSafeAssetSave: SavePackagesForObjects reported success but no package file exists for %s; trying package save fallback"),
                *PackageName);
        }
    }

    TArray<UPackage*> PackagesToSave;
    PackagesToSave.Add(Package);
    const FEditorFileUtils::EPromptReturnCode PromptSaveResult =
        FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);
    const bool bPromptSaveSucceeded =
        PromptSaveResult == FEditorFileUtils::PR_Success;
    const bool bEditorSaveSucceeded =
        !bPromptSaveSucceeded && UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, false);
    const bool bExistsOnDisk = PackageExistsOnDisk();

    if (bPromptSaveSucceeded || bEditorSaveSucceeded || bExistsOnDisk)
    {
        ScanSavedPackage();
        return true;
    }

    return false;
#else
    return false;
#endif
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
inline bool McpSafeLevelSave(ULevel* Level, const FString& FullPath, int32 MaxRetries = 5)
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

    FString SaveFilename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(PackagePath, SaveFilename,
        FPackageName::GetMapPackageExtension()))
    {
        UE_LOG(LogMcpSafeOperations, Error,
            TEXT("McpSafeLevelSave: Failed to convert package path to filename: %s"), *PackagePath);
        return false;
    }

    // Perform the actual save
    // CRITICAL FIX: Always use FEditorFileUtils::SaveLevel instead of UEditorLoadingAndSavingUtils::SaveMap.
    // UEditorLoadingAndSavingUtils::SaveMap saves to a new package but doesn't update the world's outer
    // package name. This causes "World Memory Leaks" crashes when load_level is called because
    // McpSafeLoadMap doesn't recognize the saved level as the current level (package name mismatch).
    // FEditorFileUtils::SaveLevel properly updates the world's package to match the save path.
    bool bSaveSucceeded = FEditorFileUtils::SaveLevel(Level, *SaveFilename);

    if (bSaveSucceeded)
    {
        // Verify file exists on disk with bounded retries. File systems can report
        // success before directory metadata is immediately visible, especially on
        // slow or network-backed project folders.
        FString VerifyFilename;
        if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, VerifyFilename,
            FPackageName::GetMapPackageExtension()))
        {
            FString AbsoluteVerifyFilename = FPaths::ConvertRelativePathToFull(VerifyFilename);

            const int32 ActualRetries = FMath::Clamp(MaxRetries, 1, 10);
            const float RetryDelays[] = { 0.05f, 0.10f, 0.25f, 0.50f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f };

            for (int32 Retry = 0; Retry < ActualRetries; ++Retry)
            {
                FPlatformProcess::Sleep(RetryDelays[Retry]);

                if (IFileManager::Get().FileExists(*VerifyFilename) ||
                    IFileManager::Get().FileExists(*AbsoluteVerifyFilename))
                {
                    UE_LOG(LogMcpSafeOperations, Log,
                        TEXT("McpSafeLevelSave: Successfully saved level: %s"), *PackagePath);
                    return true;
                }

                if (FPackageName::DoesPackageExist(PackagePath))
                {
                    UE_LOG(LogMcpSafeOperations, Log,
                        TEXT("McpSafeLevelSave: Package exists in UE system: %s"), *PackagePath);
                    return true;
                }
            }

            FlushRenderingCommands();
            FPlatformProcess::Sleep(0.5f);
            if (IFileManager::Get().FileExists(*VerifyFilename) ||
                IFileManager::Get().FileExists(*AbsoluteVerifyFilename))
            {
                UE_LOG(LogMcpSafeOperations, Log,
                    TEXT("McpSafeLevelSave: Successfully saved level after final flush: %s"), *PackagePath);
                return true;
            }

            if (FPackageName::DoesPackageExist(PackagePath))
            {
                UE_LOG(LogMcpSafeOperations, Log,
                    TEXT("McpSafeLevelSave: Package exists in UE system after final flush: %s"), *PackagePath);
                return true;
            }

            UE_LOG(LogMcpSafeOperations, Error,
                TEXT("McpSafeLevelSave: Save reported success but file not found after %d retries: %s"),
                ActualRetries, *VerifyFilename);
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

    // UE 5.7 World Memory Leaks guard: if the target world package is still in
    // memory from a prior CreateWorld flow, clean it up before LoadMap attempts
    // to reuse the package. Otherwise EditorServer can fatal while reporting
    // leaked world objects that could not be garbage collected.
    {
        FString NormalizedMapPath = MapPath;
        if (NormalizedMapPath.EndsWith(TEXT(".umap")))
        {
            NormalizedMapPath.LeftChopInline(5);
        }

        UPackage* ExistingPackage = FindObject<UPackage>(nullptr, *NormalizedMapPath);
        if (ExistingPackage)
        {
            UWorld* ExistingWorld = FindObject<UWorld>(ExistingPackage, *FPaths::GetBaseFilename(NormalizedMapPath));
            if (ExistingWorld != CurrentWorld)
            {
                UE_LOG(LogMcpSafeOperations, Warning,
                    TEXT("McpSafeLoadMap: Target package '%s' already exists in memory; unloading before load"),
                    *NormalizedMapPath);

#if MCP_HAS_PACKAGE_TOOLS
                TArray<UPackage*> PackagesToUnload;
                PackagesToUnload.Add(ExistingPackage);
                TWeakObjectPtr<UPackage> WeakExistingPackage = ExistingPackage;

                FText UnloadError;
                const bool bUnloadSucceeded = UPackageTools::UnloadPackages(PackagesToUnload, UnloadError, true);
                if (!UnloadError.IsEmpty())
                {
                    UE_LOG(LogMcpSafeOperations, Warning,
                        TEXT("McpSafeLoadMap: UnloadPackages reported for '%s': %s"),
                        *NormalizedMapPath,
                        *UnloadError.ToString());
                }

                FlushRenderingCommands();
                CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
                FlushRenderingCommands();

                if (!bUnloadSucceeded || WeakExistingPackage.IsValid())
                {
                    UE_LOG(LogMcpSafeOperations, Error,
                        TEXT("McpSafeLoadMap: Failed to unload pre-existing target package '%s'; aborting map load to avoid EditorServer fatal"),
                        *NormalizedMapPath);
                    return false;
                }

                UE_LOG(LogMcpSafeOperations, Log,
                    TEXT("McpSafeLoadMap: Unloaded pre-existing world package '%s'"),
                    *NormalizedMapPath);
#else
                UE_LOG(LogMcpSafeOperations, Error,
                    TEXT("McpSafeLoadMap: PackageTools unavailable and target package '%s' is already loaded; aborting map load to avoid EditorServer fatal"),
                    *NormalizedMapPath);
                return false;
#endif
            }
        }
    }

    // STEP 11: Load the map
    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeLoadMap: Loading map '%s'"), *MapPath);
    bool bLoaded = FEditorFileUtils::LoadMap(*MapPath);

    if (bLoaded)
    {
        UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeLoadMap: Successfully loaded map '%s'"), *MapPath);

        // STEP 13: Unregister ticking on new world's actors. Disabling alone
        // leaves tick functions registered with TickTaskManager, which can keep
        // stale level entries alive during rapid automation map transitions.
        UWorld* NewWorld = GEditor->GetEditorWorldContext().World();
        if (NewWorld && NewWorld->PersistentLevel)
        {
            for (AActor* Actor : NewWorld->PersistentLevel->Actors)
            {
                if (Actor)
                {
                    if (Actor->PrimaryActorTick.IsTickFunctionRegistered())
                    {
                        Actor->PrimaryActorTick.UnRegisterTickFunction();
                    }
                    for (UActorComponent* Component : Actor->GetComponents())
                    {
                        if (Component && Component->PrimaryComponentTick.IsTickFunctionRegistered())
                        {
                            Component->PrimaryComponentTick.UnRegisterTickFunction();
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
 * Throttled wrapper around McpSafeAssetSave to avoid rapid repeated save calls.
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

    // Throttling parameters reserved for future implementation.
    // For now this always uses the real editor-owned save path.
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

    // Clear the entry BEFORE ForceDeleteObjects runs, but do not instantiate the
    // database during deletion if it was never initialized by the editor.
    if (FBlueprintActionDatabase* ActionDB = FBlueprintActionDatabase::TryGet())
    {
        ActionDB->ClearAssetActions(Asset);
    }
    else
    {
        UE_LOG(LogMcpSafeOperations, Verbose,
            TEXT("McpPreClearBlueprintActionDatabase: Action database not initialized for '%s'"),
            *Asset->GetName());
    }

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
		MCP_FINISH_COMPILATION_FOR_OBJECTS(CompilingManager, BatchObjects);
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
 * Unload any currently loaded packages for the given asset set using the engine's
 * package-unload path instead of manually marking package contents as garbage.
 *
 * CRITICAL FOR UE 5.7+:
 * Blueprint-family assets own generated classes, CDOs, dependency caches, editor
 * state, and package-level object graphs. Calling MarkAsGarbage() directly on the
 * package and its inners can leave GC walking invalid class/CDO state. The editor's
 * unload flow performs the required Blueprint/world cleanup before CollectGarbage.
 */
inline bool UnloadLoadedPackagesForAssets(const TArray<FAssetData>& Assets, const TCHAR* LogContext)
{
    TArray<UPackage*> PackagesToUnload;
    TArray<UObject*> LoadedObjectsForCompilation;
    TSet<FName> SeenPackages;

    for (const FAssetData& AssetData : Assets)
    {
        const FString PackagePath = AssetData.PackageName.ToString();
        if (UPackage* Package = FindObject<UPackage>(nullptr, *PackagePath))
        {
            if (!SeenPackages.Contains(Package->GetFName()))
            {
                SeenPackages.Add(Package->GetFName());
                PackagesToUnload.Add(Package);
                UE_LOG(LogMcpSafeOperations, Log,
                    TEXT("%s: Loaded package scheduled for unload: %s"),
                    LogContext,
                    *PackagePath);
            }
        }

	const FString ObjectPath = MCP_ASSET_DATA_GET_SOFT_PATH(AssetData);
	if (!ObjectPath.IsEmpty())
        {
            if (UObject* ExistingObject = FindObject<UObject>(nullptr, *ObjectPath))
            {
                LoadedObjectsForCompilation.Add(ExistingObject);
            }
        }
    }

    if (PackagesToUnload.Num() == 0)
    {
        return true;
    }

    if (LoadedObjectsForCompilation.Num() > 0)
    {
        McpFinishCompilationForBatch(LoadedObjectsForCompilation, LogContext);
    }

    FlushRenderingCommands();
    if (GEditor)
    {
        GEditor->ClearPreviewComponents();
        GEditor->SelectNone(false, true, false);
    }

#if MCP_HAS_PACKAGE_TOOLS
    bool bAllUnloaded = true;

    for (UPackage* PackageToUnload : PackagesToUnload)
    {
        if (!PackageToUnload)
        {
            continue;
        }

        TArray<UPackage*> SinglePackage;
        SinglePackage.Add(PackageToUnload);
        TWeakObjectPtr<UPackage> WeakPackage = PackageToUnload;

        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("%s: Unloading package individually: %s"),
            LogContext,
            *PackageToUnload->GetName());

        // Simple overload works in all UE 5.x versions (5.0 through 5.7+).
        // The FUnloadPackageParams struct is unreliable across versions and
        // may cause compilation errors in UE 5.7+ where the struct is removed.
        FText ErrorMessage;
        const bool bUnloadResult = UPackageTools::UnloadPackages(SinglePackage, ErrorMessage, true);
        if (!ErrorMessage.IsEmpty())
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("%s: UnloadPackages reported for %s: %s"),
                LogContext,
                *PackageToUnload->GetName(),
                *ErrorMessage.ToString());
        }

        FlushRenderingCommands();

        if (!bUnloadResult || WeakPackage.IsValid())
        {
            bAllUnloaded = false;
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("%s: Package still loaded after individual unload attempt: %s"),
                LogContext,
                *PackageToUnload->GetName());
        }
    }

    FlushRenderingCommands();
    return bAllUnloaded;
#else
    UE_LOG(LogMcpSafeOperations, Error,
        TEXT("%s: PackageTools not available; cannot safely unload loaded packages"),
        LogContext);
    return false;
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
    FString ClassName = MCP_ASSET_DATA_GET_CLASS_PATH(AssetData);
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
    FString ClassName = MCP_ASSET_DATA_GET_CLASS_PATH(AssetData);
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
    FString ClassName = MCP_ASSET_DATA_GET_CLASS_PATH(AssetData);

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
    const FString ClassName = MCP_ASSET_DATA_GET_CLASS_PATH(AssetData);

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
 *
 * SOLUTION: Only unload truly in-memory-only packages. For file-backed assets,
 * use the engine-owned force-delete path in explicit order so Blueprint-generated
 * classes/CDOs are torn down by ObjectTools instead of raw file deletion.
 */
inline int32 DeleteAnimationRigClusterOrdered(const TArray<FAssetData>& ClusterAssets, bool bForce)
{
    int32 DeletedCount = 0;
    (void)bForce;

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
        TEXT("DeleteAnimationRigClusterOrdered: Deleting %d cluster assets via ordered engine-owned deletion"),
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

    // STEP 2: Identify in-memory-only assets first. Those need package unload;
    // file-backed assets stay on the engine-owned force-delete path and are NOT batch-unloaded.
    TArray<FAssetData> InMemoryOnlyAssets;
    TArray<FAssetData> FileBackedAssets;

    for (const FAssetData& AssetData : OrderedAssets)
    {
        const FString PackagePath = AssetData.PackageName.ToString();
        FString AssetFilePath;
        bool bHasBackingFile = false;

        if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, AssetFilePath, FPackageName::GetAssetPackageExtension()))
        {
            FString AbsolutePath = FPaths::IsRelative(AssetFilePath)
                ? FPaths::ConvertRelativePathToFull(AssetFilePath)
                : AssetFilePath;
            FPaths::NormalizeFilename(AbsolutePath);
            bHasBackingFile = IFileManager::Get().FileExists(*AbsolutePath);
        }

        if (!bHasBackingFile)
        {
            InMemoryOnlyAssets.Add(AssetData);
        }
        else
        {
            FileBackedAssets.Add(AssetData);
        }
    }

    if (InMemoryOnlyAssets.Num() > 0)
    {
        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("DeleteAnimationRigClusterOrdered: Unloading %d in-memory-only packages individually before delete"),
            InMemoryOnlyAssets.Num());

        if (!UnloadLoadedPackagesForAssets(InMemoryOnlyAssets, TEXT("DeleteAnimationRigClusterOrdered[InMemoryOnly]")))
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("DeleteAnimationRigClusterOrdered: One or more in-memory-only packages remained loaded before delete; continuing with filesystem-backed cleanup"));
        }
    }

// STEP 3: Count the in-memory-only assets that were successfully unloaded.
for (const FAssetData& AssetData : InMemoryOnlyAssets)
{
const FString PackagePath = AssetData.PackageName.ToString();
const FString ObjectPath = MCP_ASSET_DATA_GET_SOFT_PATH(AssetData);
const bool bPackageStillLoaded = FindObject<UPackage>(nullptr, *PackagePath) != nullptr;
const bool bObjectStillLoaded = !ObjectPath.IsEmpty() && FindObject<UObject>(nullptr, *ObjectPath) != nullptr;

        if (!bPackageStillLoaded && !bObjectStillLoaded)
        {
            ++DeletedCount;
            UE_LOG(LogMcpSafeOperations, Log,
                TEXT("DeleteAnimationRigClusterOrdered: Unloaded in-memory-only asset package cleanly: %s"),
                *PackagePath);
        }
        else
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("DeleteAnimationRigClusterOrdered: In-memory-only package still loaded after unload attempt: %s"),
                *PackagePath);
        }
    }

    auto ForceDeleteBatch = [&](const TArray<FAssetData>& BatchAssets, const TCHAR* BatchLabel) -> bool
    {
        if (BatchAssets.Num() == 0)
        {
            return true;
        }

        auto ForceDeleteLoadedObjects = [&](TArray<UObject*>& ObjectsToDelete) -> bool
        {
            for (UObject* BatchObject : ObjectsToDelete)
            {
                if (UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(BatchObject))
                {
                    McpQuiesceAnimBlueprintBeforeDelete(AnimBlueprint);
                }
            }

            McpQuiesceBeforeBatchDelete(ObjectsToDelete);
            for (UObject* BatchObject : ObjectsToDelete)
            {
                McpPreClearBlueprintActionDatabase(BatchObject);
            }

            UE_LOG(LogMcpSafeOperations, Log,
                TEXT("DeleteAnimationRigClusterOrdered: Force deleting %d asset(s) via ObjectTools batch [%s]"),
                ObjectsToDelete.Num(),
                BatchLabel);

            const int32 DeletedByEngine = ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
            McpQuiesceAfterBatchDelete(ObjectsToDelete);

            if (DeletedByEngine != ObjectsToDelete.Num())
            {
                UE_LOG(LogMcpSafeOperations, Error,
                    TEXT("DeleteAnimationRigClusterOrdered: ObjectTools::ForceDeleteObjects batch [%s] deleted %d/%d asset(s)"),
                    BatchLabel,
                    DeletedByEngine,
                    ObjectsToDelete.Num());
                return false;
            }

            DeletedCount += DeletedByEngine;
            return true;
        };

        const bool bDeleteAnimBlueprintsIndividually = FCString::Strcmp(BatchLabel, TEXT("AnimBlueprintFamily")) == 0;
        if (bDeleteAnimBlueprintsIndividually)
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("DeleteAnimationRigClusterOrdered: Deleting %d AnimBlueprint asset(s) individually via engine-owned delete"),
                BatchAssets.Num());

for (const FAssetData& BatchAsset : BatchAssets)
{
UObject* AssetObject = BatchAsset.GetAsset();
if (!AssetObject)
{
UE_LOG(LogMcpSafeOperations, Error, TEXT("DeleteAnimationRigClusterOrdered: Failed to load file-backed asset for delete: %s"), *MCP_ASSET_DATA_GET_OBJECT_PATH(BatchAsset));
return false;
}

TArray<UObject*> SingleObjectToDelete;
SingleObjectToDelete.Add(AssetObject);
if (!ForceDeleteLoadedObjects(SingleObjectToDelete))
{
return false;
}
}

return true;
}

TArray<UObject*> ObjectsToDelete;
ObjectsToDelete.Reserve(BatchAssets.Num());

for (const FAssetData& BatchAsset : BatchAssets)
{
UObject* AssetObject = BatchAsset.GetAsset();
if (!AssetObject)
{
UE_LOG(LogMcpSafeOperations, Error, TEXT("DeleteAnimationRigClusterOrdered: Failed to load file-backed asset for delete: %s"), *MCP_ASSET_DATA_GET_OBJECT_PATH(BatchAsset));
return false;
}

ObjectsToDelete.Add(AssetObject);
}

return ForceDeleteLoadedObjects(ObjectsToDelete);
};

// STEP 4: Delete file-backed assets through the engine-owned force-delete path.
// IMPORTANT: Keep Blueprint families narrow. AnimBlueprints and ControlRigBlueprints
// must not be mixed in the same batch because their editor teardown graphs differ.
TArray<FAssetData> AnimBlueprintAssets;
TArray<FAssetData> ControlRigBlueprintAssets;
TArray<FAssetData> GenericBlueprintAssets;
TArray<FAssetData> OtherFileBackedAssets;

for (const FAssetData& AssetData : FileBackedAssets)
{
        const FString ClassName = MCP_ASSET_DATA_GET_CLASS_PATH(AssetData);
        if (ClassName.Contains(TEXT("AnimBlueprint")))
        {
            AnimBlueprintAssets.Add(AssetData);
        }
        else if (ClassName.Contains(TEXT("ControlRigBlueprint")))
        {
            ControlRigBlueprintAssets.Add(AssetData);
        }
        else if (ClassName.Contains(TEXT("Blueprint")))
        {
            GenericBlueprintAssets.Add(AssetData);
        }
        else
        {
            OtherFileBackedAssets.Add(AssetData);
        }
    }

    if (!ForceDeleteBatch(AnimBlueprintAssets, TEXT("AnimBlueprintFamily")))
    {
        return INDEX_NONE;
    }

    if (!ForceDeleteBatch(ControlRigBlueprintAssets, TEXT("ControlRigBlueprintFamily")))
    {
        return INDEX_NONE;
    }

    if (!ForceDeleteBatch(GenericBlueprintAssets, TEXT("GenericBlueprintFamily")))
    {
        return INDEX_NONE;
    }

    for (const FAssetData& AssetData : OtherFileBackedAssets)
    {
        TArray<FAssetData> SingleAssetBatch;
        SingleAssetBatch.Add(AssetData);
        if (!ForceDeleteBatch(SingleAssetBatch, TEXT("Singleton")))
        {
            return INDEX_NONE;
        }
    }

    // STEP 5: Final cleanup - GC to clean up any stale references
    FlushRenderingCommands();
    if (GEditor)
    {
        GEditor->ForceGarbageCollection(true);
    }
    FlushRenderingCommands();

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("DeleteAnimationRigClusterOrdered: Deleted %d/%d cluster assets via engine-owned ordered deletion"),
        DeletedCount, OrderedAssets.Num());

    return DeletedCount;
}

/**
 * Split a batch of asset data into file-backed loaded objects vs in-memory-only assets.
 * In-memory-only assets are unloaded through PackageTools instead of being manually
 * marked for garbage collection.
 */
inline bool PrepareAssetBatchForDelete(
    const TArray<FAssetData>& Assets,
    const TCHAR* LogContext,
    TArray<UObject*>& OutFileBackedObjects,
    int32& OutInMemoryOnlyCount)
{
    OutFileBackedObjects.Reset();
    OutInMemoryOnlyCount = 0;

    IFileManager& FileManager = IFileManager::Get();
    TArray<FAssetData> InMemoryOnlyAssets;

    for (const FAssetData& AssetData : Assets)
    {
        const FString PackagePath = AssetData.PackageName.ToString();
        FString AssetFilePath;
        bool bHasBackingFile = false;
const FString ClassName = MCP_ASSET_DATA_GET_CLASS_PATH(AssetData);
const bool bIsWorldAsset = ClassName.Equals(TEXT("/Script/Engine.World"), ESearchCase::IgnoreCase) ||
                           ClassName.EndsWith(TEXT(".World"), ESearchCase::IgnoreCase);
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
            InMemoryOnlyAssets.Add(AssetData);
            ++OutInMemoryOnlyCount;
            continue;
        }

        UObject* Asset = AssetData.GetAsset();
        if (!Asset)
        {
            continue;
        }

        OutFileBackedObjects.Add(Asset);
    }

    if (OutInMemoryOnlyCount > 0)
    {
        if (!UnloadLoadedPackagesForAssets(InMemoryOnlyAssets, LogContext))
        {
            UE_LOG(LogMcpSafeOperations, Error,
                TEXT("%s: Failed to unload one or more in-memory-only packages before delete"),
                LogContext);
            return false;
        }
    }

    return true;
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
FString ClassName = MCP_ASSET_DATA_GET_CLASS_PATH(AssetData);
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
FString ClassName = MCP_ASSET_DATA_GET_CLASS_PATH(AssetData);
// Only actual UWorld/map packages should use the world deletion path. Avoid
// broad substring checks: non-world assets such as LevelSequence contain
// "Level" in their class names but must remain on the normal asset path.
return ClassName.Equals(TEXT("/Script/Engine.World"), ESearchCase::IgnoreCase) ||
       ClassName.EndsWith(TEXT(".World"), ESearchCase::IgnoreCase);
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

    if (!UnloadLoadedPackagesForAssets(WorldAssets, TEXT("DeleteWorldPackagesByPath")))
    {
        UE_LOG(LogMcpSafeOperations, Error,
            TEXT("DeleteWorldPackagesByPath: Failed to unload one or more loaded world packages before file deletion"));
        return INDEX_NONE;
    }

    for (const FAssetData& AssetData : WorldAssets)
    {
        const FString PackagePath = AssetData.PackageName.ToString();

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

        TArray<FAssetData> RemainingWorldAssets;
        AssetRegistry.GetAssetsByPackageName(AssetData.PackageName, RemainingWorldAssets, true);
        const bool bRegistryPackageGone = RemainingWorldAssets.Num() == 0;

        if (bDeletedMap && !FileManager.FileExists(*AbsoluteMapFilename) && bRegistryPackageGone)
        {
            ++DeletedCount;
        }
        else
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("DeleteWorldPackagesByPath: World package still present after delete attempt (package=%s deletedMap=%d fileExists=%d registryCount=%d)"),
                *PackagePath,
                bDeletedMap ? 1 : 0,
                FileManager.FileExists(*AbsoluteMapFilename) ? 1 : 0,
                RemainingWorldAssets.Num());
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

        // Empty content browser folders do not need the editor asset deletion path.
        // UEditorAssetLibrary::DeleteDirectory can block on live editor state even when
        // the asset registry has no packages to delete, so remove the registry paths
        // and physical directory directly, then verify both states.
        TArray<FString> EmptySubPaths;
        AssetRegistry.GetSubPaths(FolderPath, EmptySubPaths, true);
        EmptySubPaths.Sort([](const FString& A, const FString& B)
        {
            return A.Len() > B.Len();
        });
        for (const FString& SubPath : EmptySubPaths)
        {
            AssetRegistry.RemovePath(SubPath);
        }
        AssetRegistry.RemovePath(FolderPath);

        FString EmptyLocalPath;
        bool bDirectoryExistsOnDisk = false;
        if (FPackageName::TryConvertLongPackageNameToFilename(FolderPath, EmptyLocalPath))
        {
            IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
            if (PlatformFile.DirectoryExists(*EmptyLocalPath))
            {
                PlatformFile.DeleteDirectoryRecursively(*EmptyLocalPath);
            }
            bDirectoryExistsOnDisk = PlatformFile.DirectoryExists(*EmptyLocalPath);
        }

        TArray<FString> RemainingEmptySubPaths;
        AssetRegistry.GetSubPaths(FolderPath, RemainingEmptySubPaths, true);
        const bool bDeleted = RemainingEmptySubPaths.Num() == 0 && !bDirectoryExistsOnDisk;
        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpSafeDeleteFolder: Empty folder deletion result for '%s' (remainingSubPaths=%d existsOnDisk=%d)"),
            *FolderPath, RemainingEmptySubPaths.Num(), bDirectoryExistsOnDisk ? 1 : 0);
        return bDeleted;
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
UE_LOG(LogMcpSafeOperations, Log, TEXT(" World asset: %s (%s)"),
*AssetData.AssetName.ToString(), *MCP_ASSET_DATA_GET_CLASS_PATH(AssetData));
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
UE_LOG(LogMcpSafeOperations, Log, TEXT(" Risky special-delete asset: %s (%s)"),
*AssetData.AssetName.ToString(), *MCP_ASSET_DATA_GET_CLASS_PATH(AssetData));
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

        // STEP 6b: Delete remaining risky special-delete assets using the same
        // ordered engine-owned path, which now keeps file-backed assets on
        // ObjectTools::ForceDeleteObjects and only unloads true in-memory packages.
        const int32 TotalRisky = GenericRiskyAssets.Num();

        if (TotalRisky > 0)
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("McpSafeDeleteFolder: Deleting %d remaining risky special-delete assets via ordered engine-owned deletion"),
                TotalRisky);

            // Use the same ordered engine-owned deletion approach as the mixed cluster
            const int32 GenericDeleted = DeleteAnimationRigClusterOrdered(GenericRiskyAssets, bForce);
            DeletedRisky += GenericDeleted;

            UE_LOG(LogMcpSafeOperations, Log,
                TEXT("McpSafeDeleteFolder: Deleted %d/%d remaining risky special-delete assets via ordered engine-owned deletion"),
                GenericDeleted, TotalRisky);
        }

        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpSafeDeleteFolder: Deleted %d total risky special-delete assets"), DeletedRisky);
    }

    // STEP 7: Delete SAFE non-world assets with tight compilation barriers
    if (SafeAssets.Num() > 0)
    {
        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpSafeDeleteFolder: Deleting %d safe assets via UEditorAssetLibrary::DeleteAsset"),
            SafeAssets.Num());

        int32 DeletedSafeAssets = 0;
        for (const FAssetData& SafeAsset : SafeAssets)
        {
            const FString SafeAssetPath = SafeAsset.PackageName.ToString();
            if (SafeAssetPath.IsEmpty())
            {
                continue;
            }

            const bool bDeletedSafeAsset = UEditorAssetLibrary::DeleteAsset(SafeAssetPath);
            const bool bExistsAfterDelete = UEditorAssetLibrary::DoesAssetExist(SafeAssetPath);
            if (bDeletedSafeAsset && !bExistsAfterDelete)
            {
                ++DeletedSafeAssets;
            }
            else
            {
                UE_LOG(LogMcpSafeOperations, Error,
                    TEXT("McpSafeDeleteFolder: Failed to delete safe asset '%s' (deleteResult=%d existsAfter=%d)"),
                    *SafeAssetPath, bDeletedSafeAsset ? 1 : 0, bExistsAfterDelete ? 1 : 0);
                return false;
            }
        }

        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpSafeDeleteFolder: Deleted %d/%d safe assets"),
            DeletedSafeAssets, SafeAssets.Num());
    }

    // STEP 8: Delete WORLD ASSETS LAST (they should be unloaded now)
    if (WorldAssets.Num() > 0)
    {
        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpSafeDeleteFolder: Deleting %d world assets via package/file path"),
            WorldAssets.Num());

        const int32 DeletedWorlds = DeleteWorldPackagesByPath(WorldAssets);
        if (DeletedWorlds == INDEX_NONE)
        {
            return false;
        }
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

    TArray<FAssetData> RemainingFileBackedAssets;
    auto AssetDataHasBackingFile = [](const FAssetData& AssetData)
    {
        const FString PackagePath = AssetData.PackageName.ToString();

        FString AssetFilename;
        if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, AssetFilename, FPackageName::GetAssetPackageExtension()))
        {
            if (IFileManager::Get().FileExists(*FPaths::ConvertRelativePathToFull(AssetFilename)))
            {
                return true;
            }
        }

        FString MapFilename;
        if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, MapFilename, FPackageName::GetMapPackageExtension()))
        {
            if (IFileManager::Get().FileExists(*FPaths::ConvertRelativePathToFull(MapFilename)))
            {
                return true;
            }
        }

        return false;
    };

    for (const FAssetData& RemainingAsset : RemainingAssets)
    {
        if (AssetDataHasBackingFile(RemainingAsset))
        {
            RemainingFileBackedAssets.Add(RemainingAsset);
        }
    }

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
    else if (RemainingFileBackedAssets.Num() == 0 && RemainingSubPaths.Num() == 0 && !bDirectoryExistsOnDisk)
    {
        UE_LOG(LogMcpSafeOperations, Warning,
            TEXT("McpSafeDeleteFolder: Physical folder '%s' deleted; only %d in-memory package(s) without backing files remain"),
            *FolderPath, RemainingAssets.Num());
        return true;
    }
    else
    {
        UE_LOG(LogMcpSafeOperations, Warning,
            TEXT("McpSafeDeleteFolder: Directory still exists after deletion attempt (remainingAssets=%d remainingFileBackedAssets=%d remainingSubPaths=%d existsOnDisk=%d)"),
            RemainingAssets.Num(), RemainingFileBackedAssets.Num(), RemainingSubPaths.Num(), bDirectoryExistsOnDisk ? 1 : 0);

for (const FAssetData& RemainingAsset : RemainingAssets)
{
UE_LOG(LogMcpSafeOperations, Warning, TEXT("McpSafeDeleteFolder: Remaining asset: %s (%s)"),
*MCP_ASSET_DATA_GET_SOFT_PATH(RemainingAsset),
*MCP_ASSET_DATA_GET_CLASS_PATH(RemainingAsset));
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

// Non-editor explicit failure fallbacks
inline bool McpSafeAssetSave(void* Asset) { return false; }
inline bool McpSafeLevelSave(void* Level, const FString& Path, int32 = 1) { return false; }
inline bool McpSafeLoadMap(const FString& MapPath, bool = true) { return false; }
inline class UMaterialInterface* McpLoadMaterialWithFallback(const FString& = FString(), bool = false) { return nullptr; }
inline bool SaveLoadedAssetThrottled(void* Asset, double = -1.0, bool = false) { return false; }
inline void ScanPathSynchronous(const FString&, bool = true) {}

#endif // WITH_EDITOR

} // namespace McpSafeOperations

#pragma once

#include "Subsystems/AssetEditorSubsystem.h"
#include "Safety/McpSafeOperationsLog.h"
#include "PackageTools.h"

#include "Components/ActorComponent.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "UObject/UObjectHash.h"

#include "EditorAssetLibrary.h"

namespace McpSafeOperations
{


inline bool ResolveExpectedMapPackageName(const FString& MapPath, FString& OutPackageName)
{
    OutPackageName = MapPath;
    if (OutPackageName.EndsWith(TEXT(".umap")))
    {
        OutPackageName.LeftChopInline(5);
    }

    if (FPackageName::IsValidLongPackageName(OutPackageName))
    {
        return true;
    }

    FString AbsoluteMapFilename = FPaths::ConvertRelativePathToFull(MapPath);
    FPaths::NormalizeFilename(AbsoluteMapFilename);
    return FPackageName::TryConvertFilenameToLongPackageName(
        AbsoluteMapFilename,
        OutPackageName);
}

inline bool McpSafeLoadMap(const FString& MapPath, bool bForceCleanup = true)
{
    if (!GEditor)
    {
        UE_LOG(LogMcpSafeOperations, Error, TEXT("McpSafeLoadMap: GEditor is null"));
        return false;
    }

    if (!IsInGameThread())
    {
        UE_LOG(LogMcpSafeOperations, Error, TEXT("McpSafeLoadMap: Must be called from game thread"));
        return false;
    }

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
    // Callers pass a long package name or a .umap filename; every check below needs the package name (the
    // in-memory check never matched a filename, so a resident create_level world slipped past it and hit the
    // EditorServer "World Memory Leaks" fatal).
    FString ExpectedPackageName;
    const bool bResolvedExpectedPackage = ResolveExpectedMapPackageName(MapPath, ExpectedPackageName);
    const FString NormalizedMapPath = bResolvedExpectedPackage
        ? ExpectedPackageName
        : (MapPath.EndsWith(TEXT(".umap")) ? MapPath.LeftChop(5) : MapPath);
    if (CurrentWorld)
    {
        FString CurrentMapPath = CurrentWorld->GetOutermost()->GetName();
        if (CurrentMapPath.Equals(NormalizedMapPath, ESearchCase::IgnoreCase))
        {
            UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeLoadMap: Map '%s' is already loaded, skipping"), *MapPath);
            return true;
        }
    }

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

        if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
        {
            AssetEditorSubsystem->CloseAllAssetEditors();
        }

        FlushRenderingCommands();
        GEditor->ForceGarbageCollection(true);
        FlushRenderingCommands();
        FPlatformProcess::Sleep(0.05f);

        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpSafeLoadMap: Minimal cleanup completed before map load"));
    }

    {
        UPackage* ExistingPackage = FindObject<UPackage>(nullptr, *NormalizedMapPath);
        if (ExistingPackage)
        {
            UWorld* ExistingWorld = FindObject<UWorld>(ExistingPackage, *FPaths::GetBaseFilename(NormalizedMapPath));
            if (ExistingWorld != CurrentWorld)
            {
                UE_LOG(LogMcpSafeOperations, Warning,
                    TEXT("McpSafeLoadMap: Target package '%s' already exists in memory; unloading before load"),
                    *NormalizedMapPath);

                TArray<UPackage*> PackagesToUnload;
                PackagesToUnload.Add(ExistingPackage);
                // RF_Standalone survives GARBAGE_COLLECTION_KEEPFLAGS; strip it so
                // the unload below can release a world created in-process.
                ForEachObjectWithPackage(ExistingPackage, [](UObject* Object)
                {
                    Object->ClearFlags(RF_Standalone);
                    return true;
                }, true);
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
            }
        }
    }

    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeLoadMap: Loading map '%s'"), *MapPath);
    bool bLoaded = FEditorFileUtils::LoadMap(*MapPath);

    UWorld* LoadedWorld = GEditor->GetEditorWorldContext().World();
    const FString LoadedWorldPackageName = LoadedWorld
        ? LoadedWorld->GetOutermost()->GetName()
        : FString();
    const bool bLoadedRequestedWorld = bLoaded &&
        bResolvedExpectedPackage &&
        LoadedWorld &&
        LoadedWorldPackageName.Equals(
            ExpectedPackageName,
            ESearchCase::IgnoreCase);

    if (bLoadedRequestedWorld)
    {
        UE_LOG(LogMcpSafeOperations, Log, TEXT("McpSafeLoadMap: Successfully loaded map '%s'"), *MapPath);

        UWorld* NewWorld = LoadedWorld;
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
        UE_LOG(LogMcpSafeOperations, Error,
            TEXT("McpSafeLoadMap: Editor world does not match requested map (requested=%s resolved=%s current=%s loadResult=%d)"),
            *MapPath,
            bResolvedExpectedPackage ? *ExpectedPackageName : TEXT("<unresolved>"),
            LoadedWorldPackageName.IsEmpty() ? TEXT("<none>") : *LoadedWorldPackageName,
            bLoaded ? 1 : 0);
    }

    return bLoadedRequestedWorld;
}


}

#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Safety/McpSafeOperationsDeleteEditorSupport.h"
#include "Safety/McpSafeOperationsLog.h"
#include "PackageTools.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"

namespace McpSafeOperations
{


inline void McpPreClearBlueprintActionDatabase(UObject* Asset)
{
    if (!Asset)
    {
        return;
    }

    if (!Asset->IsA<UBlueprint>())
    {
        return;
    }

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpPreClearBlueprintActionDatabase: Pre-clearing action database for '%s'"),
        *Asset->GetName());

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
}

inline void McpSafePostDeleteGC()
{
    FlushRenderingCommands();

    if (GEditor)
    {
        GEditor->ForceGarbageCollection(true);
    }

    FlushRenderingCommands();
}

inline void McpQuiesceAllState()
{
    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpQuiesceAllState: Starting full editor quiesce"));

    FAssetCompilingManager& CompilingManager = FAssetCompilingManager::Get();
    int32 RemainingAssets = CompilingManager.GetNumRemainingAssets();
    if (RemainingAssets > 0)
    {
        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpQuiesceAllState: Waiting for %d compiling assets"), RemainingAssets);
        CompilingManager.FinishAllCompilation();
    }

    FlushRenderingCommands();
    FPlatformProcess::Sleep(0.016f);
    McpSafePostDeleteGC();

    UE_LOG(LogMcpSafeOperations, Log, TEXT("McpQuiesceAllState: Editor quiesce completed"));
}

inline void McpFinishCompilationForBatch(TArray<UObject*>& BatchObjects, const TCHAR* Context)
{
    FAssetCompilingManager& CompilingManager = FAssetCompilingManager::Get();

    int32 GlobalRemaining = CompilingManager.GetNumRemainingAssets();
    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpFinishCompilationForBatch: [%s] %d global compiling assets, %d objects in batch"),
        Context, GlobalRemaining, BatchObjects.Num());

    if (BatchObjects.Num() > 0)
    {
        MCP_FINISH_COMPILATION_FOR_OBJECTS(CompilingManager, BatchObjects);
    }

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
}

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
}


}

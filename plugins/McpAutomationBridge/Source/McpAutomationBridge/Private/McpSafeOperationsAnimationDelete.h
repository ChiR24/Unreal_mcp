#pragma once

#include "McpSafeOperationsAssetClassification.h"
#include "McpSafeOperationsDeleteQuiesce.h"

namespace McpSafeOperations
{

#if WITH_EDITOR

inline int32 DeleteAnimationRigClusterOrdered(const TArray<FAssetData>& ClusterAssets, bool bForce)
{
    int32 DeletedCount = 0;
    (void)bForce;

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

#if MCP_HAS_ASSET_EDITOR_SUBSYSTEM
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    if (AssetEditorSubsystem)
    {
        AssetEditorSubsystem->CloseAllAssetEditors();
        UE_LOG(LogMcpSafeOperations, Log, TEXT("DeleteAnimationRigClusterOrdered: Closed all asset editors"));
    }
#endif

    if (GEditor)
    {
        GEditor->ClearPreviewComponents();
        GEditor->SelectNone(false, true, false);
    }

    FlushRenderingCommands();
    if (GEditor)
    {
        GEditor->ForceGarbageCollection(true);
    }
    FlushRenderingCommands();
    FPlatformProcess::Sleep(0.1f);

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

#endif

}

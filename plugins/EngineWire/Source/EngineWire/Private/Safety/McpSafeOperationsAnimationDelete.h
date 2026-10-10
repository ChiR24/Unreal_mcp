#pragma once

#include "Safety/McpSafeOperationsAssetClassification.h"
#include "Safety/McpSafeOperationsAssetSave.h"
#include "Safety/McpSafeOperationsDeleteQuiesce.h"

namespace McpSafeOperations
{


inline int32 DeleteAnimationRigClusterOrdered(const TArray<FAssetData>& ClusterAssets)
{
    int32 DeletedCount = 0;

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

    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    if (AssetEditorSubsystem)
    {
        AssetEditorSubsystem->CloseAllAssetEditors();
        UE_LOG(LogMcpSafeOperations, Log, TEXT("DeleteAnimationRigClusterOrdered: Closed all asset editors"));
    }

    if (GEditor)
    {
        GEditor->ClearPreviewComponents();
        GEditor->SelectNone(false, true, false);
    }

    McpSafePostDeleteGC();
    FPlatformProcess::Sleep(0.1f);

    TArray<FAssetData> InMemoryOnlyAssets;
    TArray<FAssetData> FileBackedAssets;

    for (const FAssetData& AssetData : OrderedAssets)
    {
        (McpPackageHasBackingFile(AssetData.PackageName.ToString()) ? FileBackedAssets : InMemoryOnlyAssets).Add(AssetData);
    }

    // Unsaved assets go through the engine's force delete with the rest below. Unloading their packages
    // first could not take one another asset still held (it stayed behind while the folder delete answered
    // success), and unloading a package something still references can leave stale pointers behind.
    TArray<FAssetData> InMemoryStillLoaded;
    for (const FAssetData& AssetData : InMemoryOnlyAssets)
    {
        const FString ObjectPath = MCP_ASSET_DATA_GET_SOFT_PATH(AssetData);
        if (!ObjectPath.IsEmpty() && IsValid(FindObject<UObject>(nullptr, *ObjectPath)))
        {
            InMemoryStillLoaded.Add(AssetData);
        }
        else
        {
            ++DeletedCount; // only an object already marked for collection is left
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

        const auto LoadForDelete = [](const FAssetData& BatchAsset) -> UObject*
        {
            UObject* AssetObject = BatchAsset.GetAsset();
            if (!AssetObject)
            {
                UE_LOG(LogMcpSafeOperations, Error, TEXT("DeleteAnimationRigClusterOrdered: Failed to load file-backed asset for delete: %s"), *MCP_ASSET_DATA_GET_OBJECT_PATH(BatchAsset));
            }
            return AssetObject;
        };

        const bool bDeleteAnimBlueprintsIndividually = FCString::Strcmp(BatchLabel, TEXT("AnimBlueprintFamily")) == 0;
        if (bDeleteAnimBlueprintsIndividually)
        {
            UE_LOG(LogMcpSafeOperations, Warning,
                TEXT("DeleteAnimationRigClusterOrdered: Deleting %d AnimBlueprint asset(s) individually via engine-owned delete"),
                BatchAssets.Num());

            for (const FAssetData& BatchAsset : BatchAssets)
            {
                UObject* AssetObject = LoadForDelete(BatchAsset);
                TArray<UObject*> SingleObjectToDelete{AssetObject};
                if (!AssetObject || !ForceDeleteLoadedObjects(SingleObjectToDelete))
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
            UObject* AssetObject = LoadForDelete(BatchAsset);
            if (!AssetObject)
            {
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

    if (!ForceDeleteBatch(InMemoryStillLoaded, TEXT("InMemoryOnlyLeftovers")))
    {
        return INDEX_NONE;
    }

    McpSafePostDeleteGC();

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("DeleteAnimationRigClusterOrdered: Deleted %d/%d cluster assets via engine-owned ordered deletion"),
        DeletedCount, OrderedAssets.Num());

    return DeletedCount;
}


}

#pragma once

#include "Safety/McpSafeOperationsDeleteCompilation.h"

namespace McpSafeOperations
{


inline void McpQuiesceBeforeBatchDelete(TArray<UObject*>& BatchObjects)
{
    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpQuiesceBeforeBatchDelete: Starting pre-delete quiesce for %d objects"),
        BatchObjects.Num());

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

    McpFinishCompilationForBatch(BatchObjects, TEXT("pre-delete"));
    FlushRenderingCommands();
    FPlatformProcess::Sleep(0.016f);
    McpSafePostDeleteGC();

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpQuiesceBeforeBatchDelete: Pre-delete quiesce complete"));
}

inline void McpQuiesceAfterBatchDelete(const TArray<UObject*>& BatchObjects)
{
    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpQuiesceAfterBatchDelete: Starting post-delete quiesce for %d objects"),
        BatchObjects.Num());

    FlushRenderingCommands();
    FPlatformProcess::Sleep(0.016f);
    McpSafePostDeleteGC();

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpQuiesceAfterBatchDelete: Post-delete quiesce complete"));
}

inline void McpQuiesceAnimBlueprintBeforeDelete(UAnimBlueprint* AnimBlueprint)
{
    if (!AnimBlueprint)
    {
        return;
    }

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpQuiesceAnimBlueprintBeforeDelete: Starting AnimBlueprint-specific quiesce for '%s'"),
        *AnimBlueprint->GetName());

    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    if (AssetEditorSubsystem)
    {
        AssetEditorSubsystem->CloseAllEditorsForAsset(AnimBlueprint);

        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpQuiesceAnimBlueprintBeforeDelete: Closed all editors for '%s'"),
            *AnimBlueprint->GetName());
    }

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

        GEditor->SelectNone(false, true, false);
    }

    FlushRenderingCommands();
    FPlatformProcess::Sleep(0.050f);
    FlushRenderingCommands();

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpQuiesceAnimBlueprintBeforeDelete: AnimBlueprint-specific quiesce complete for '%s'"),
        *AnimBlueprint->GetName());
}


}

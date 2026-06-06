#pragma once

#include "McpSafeOperationsDeleteCompilation.h"

namespace McpSafeOperations
{

#if WITH_EDITOR

inline void McpQuiesceBeforeBatchDelete(TArray<UObject*>& BatchObjects)
{
    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpQuiesceBeforeBatchDelete: Starting pre-delete quiesce for %d objects"),
        BatchObjects.Num());

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

    McpFinishCompilationForBatch(BatchObjects, TEXT("pre-delete"));
    FlushRenderingCommands();
    FPlatformProcess::Sleep(0.016f);
    FlushRenderingCommands();

    if (GEditor)
    {
        GEditor->ForceGarbageCollection(true);
    }

    FlushRenderingCommands();

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
    FlushRenderingCommands();

    if (GEditor)
    {
        GEditor->ForceGarbageCollection(true);
    }

    FlushRenderingCommands();

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

#if MCP_HAS_ASSET_EDITOR_SUBSYSTEM
    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>() : nullptr;
    if (AssetEditorSubsystem)
    {
        for (int32 i = 0; i < 3; ++i)
        {
            AssetEditorSubsystem->CloseAllEditorsForAsset(AnimBlueprint);
        }

        UE_LOG(LogMcpSafeOperations, Log,
            TEXT("McpQuiesceAnimBlueprintBeforeDelete: Closed all editors for '%s'"),
            *AnimBlueprint->GetName());
    }
#endif

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

        GEditor->SelectNone(false, true, false);
    }
#endif

    FlushRenderingCommands();
    FPlatformProcess::Sleep(0.050f);
    FlushRenderingCommands();

    UE_LOG(LogMcpSafeOperations, Log,
        TEXT("McpQuiesceAnimBlueprintBeforeDelete: AnimBlueprint-specific quiesce complete for '%s'"),
        *AnimBlueprint->GetName());
}

#endif

}

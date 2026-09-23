#pragma once

#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
// One build_graph step: resolve its "$alias" references, run it through the
// ordinary single-step handler, and record the outcome. Split from the batch
// loop in McpAutomationBridge_BlueprintGraphHandlersBatch.cpp.
namespace McpBlueprintGraphHandlers::GraphBatch
{
constexpr int32 MaxBatchSteps = 200;

struct FBatchState
{
    /** `id` of an earlier step -> the guid of the node it created. */
    TMap<FString, FString> Aliases;
    /** Left edge of the grid auto-placed nodes fill, right of existing nodes. */
    float OriginX = 0.0f;
    int32 AutoPlaced = 0;
};

/**
 * Runs operations[Index] and fills Entry with its outcome. Returns an empty
 * string on success, otherwise the reason, with OutErrorCode set.
 */
FString RunBatchStep(const FActionContext& Context, FBatchState& State,
                     const TSharedPtr<FJsonValue>& StepValue, int32 Index,
                     const TSharedPtr<FJsonObject>& Entry, const TSharedPtr<FJsonObject>& NodeIds,
                     FString& OutErrorCode);
}
#endif

#pragma once

#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

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

/** Synchronous, non-destructive edits only; the member steps are among them. */
bool IsBatchableEdit(const FString& Edit);

/** Every function, variable, dispatcher and async factory the steps name must resolve before
 *  any step runs; add_variable, add_function, add_event and add_event_dispatcher steps declare
 *  theirs for later steps. Empty on success, else the reason with OutIndex and OutCode set. */
FString PrecheckSteps(const FActionContext& Context, const TArray<TSharedPtr<FJsonValue>>& Steps,
                      int32& OutIndex, FString& OutCode);

/** add_variable, add_function, add_event and add_event_dispatcher run their ordinary member
 *  handler (which compiles, so later steps can use what it made). False for any other edit. */
bool RunBlueprintMemberStep(const FActionContext& Parent, const FString& Edit, const FString& StepId,
                            const TSharedPtr<FJsonObject>& Payload);

/** build_graph's body after the subAction check: checks, runs every step, and replies.
 *  bCompile false leaves the compile and the save to the caller (McpBlueprintBehaviour). */
bool RunGraphBatch(FActionContext& Context, int32 MaxSteps, bool bCompile);

/** A node of Blueprint by guid text, in any of its graphs. */
UEdGraphNode* FindBatchNode(UBlueprint* Blueprint, const FString& Guid);
}

#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersPrivate.h"

#if WITH_EDITOR
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphCompatibility.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersBatchSteps.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintDiagnostics.h"

// build_graph: one call that runs a list of graph edits. Wiring one event chain
// used to cost a round trip per node, per link and per pin default; a batch runs
// the SAME single-step handlers in-process (their replies are parked by
// FMcpResponseCaptureRegistry instead of sent), so every step keeps its checks.
// The batch stops at the first failing step; steps before it stay applied and
// their node ids come back so the caller can continue from there.
namespace McpBlueprintGraphHandlers
{
bool HandleGraphBatchAction(FActionContext& Context)
{
    using namespace GraphBatch;
    if (Context.SubAction != TEXT("build_graph"))
    {
        return false;
    }
    const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
    if (!Context.Payload->TryGetArrayField(TEXT("operations"), Steps) || Steps->Num() == 0 ||
        Steps->Num() > MaxBatchSteps)
    {
        Context.SendError(FString::Printf(
            TEXT("build_graph needs `operations`: 1-%d steps, each {edit, ...that edit's params}, "
                 "optionally `id` to name a created node for later steps as \"$id\"."),
            MaxBatchSteps), TEXT("INVALID_OPERATIONS"));
        return true;
    }

    FBatchState State;
    for (UEdGraphNode* Existing : Context.TargetGraph->Nodes)
    {
        float Width = 0.0f;
        float Height = 0.0f;
        if (Existing)
        {
            McpGraphLayout::EstimateNodeExtent(*Existing, Width, Height);
            State.OriginX = FMath::Max(State.OriginX, Existing->NodePosX + Width + 240.0f);
#if MCP_HAS_K2NODE_HEADERS
            // "$entry" is the graph's own entry node (a Construction Script's
            // exec start), which used to need an inspect_graph call to find.
            if (Existing->IsA<UK2Node_FunctionEntry>())
            {
                State.Aliases.Add(TEXT("entry"), Existing->NodeGuid.ToString());
            }
#endif
        }
    }

    TArray<TSharedPtr<FJsonValue>> Results;
    TSharedPtr<FJsonObject> NodeIds = MakeShared<FJsonObject>();
    for (int32 Index = 0; Index < Steps->Num(); ++Index)
    {
        TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetNumberField(TEXT("index"), Index);
        FString ErrorCode;
        const FString Error = RunBatchStep(Context, State, (*Steps)[Index], Index, Entry, NodeIds, ErrorCode);
        Entry->SetBoolField(TEXT("success"), Error.IsEmpty());
        Results.Add(MakeShared<FJsonValueObject>(Entry));
        if (Error.IsEmpty())
        {
            continue;
        }
        Entry->SetStringField(TEXT("error"), Error);
        TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
        Details->SetArrayField(TEXT("results"), Results);
        Details->SetObjectField(TEXT("nodeIds"), NodeIds);
        Details->SetNumberField(TEXT("succeeded"), Index);
        Details->SetNumberField(TEXT("failedIndex"), Index);
        FString Reason = Error;
        Reason.RemoveFromEnd(TEXT("."));
        Context.SendErrorWithDetails(FString::Printf(
            TEXT("build_graph stopped at operations[%d]: %s. The %d step(s) before it were applied; "
                 "their node ids are in the error detail's `nodeIds`."), Index, *Reason, Index), ErrorCode, Details);
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString FirstError;
    const bool bCompiled = McpCompileBlueprintWithDiagnostics(Context.Blueprint, Result, FirstError, 12);
    Result->SetBoolField(TEXT("saved"), SaveLoadedAssetThrottled(Context.Blueprint));
    Result->SetArrayField(TEXT("results"), Results);
    Result->SetObjectField(TEXT("nodeIds"), NodeIds);
    Result->SetNumberField(TEXT("succeeded"), Results.Num());
    Context.SendResponse(bCompiled
        ? FString::Printf(TEXT("Ran %d graph operations; the blueprint compiles."), Results.Num())
        : FString::Printf(TEXT("Ran %d graph operations. WARNING: the blueprint does not compile: %s"),
                          Results.Num(), FirstError.IsEmpty() ? TEXT("no compiler message") : *FirstError),
        Result);
    return true;
}
}
#else
namespace McpBlueprintGraphHandlers
{
bool HandleGraphBatchAction(FActionContext&)
{
    return false;
}
}
#endif

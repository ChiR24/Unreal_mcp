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
namespace
{
bool IsCallFunctionType(const FString& NodeType)
{
    return NodeType == TEXT("CallFunction") || NodeType == TEXT("K2Node_CallFunction") ||
           NodeType == TEXT("FunctionCall");
}

bool IsVariableNodeType(const FString& NodeType)
{
    return NodeType == TEXT("VariableGet") || NodeType == TEXT("VariableSet") ||
           NodeType == TEXT("K2Node_VariableGet") || NodeType == TEXT("K2Node_VariableSet");
}

// Every function and variable a step names is resolved before any step runs. A
// misspelled name at step 10 used to leave steps 0-9 applied, and the caller had
// to continue the half-built graph by node guid.
FString PrecheckSteps(const FActionContext& Context, const TArray<TSharedPtr<FJsonValue>>& Steps,
                      int32& OutIndex, FString& OutCode)
{
    const bool bWidgetBlueprint = FindObject<UObject>(Context.Blueprint, TEXT("WidgetTree")) != nullptr;
    TSet<FName> Declared;
    for (int32 Index = 0; Index < Steps.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Step = nullptr;
        if (!Steps[Index].IsValid() || !Steps[Index]->TryGetObject(Step) || Step == nullptr)
        {
            continue;
        }
        FString Edit, NodeType, Member, MemberClass;
        (*Step)->TryGetStringField(TEXT("edit"), Edit);
        (*Step)->TryGetStringField(TEXT("nodeType"), NodeType);
        (*Step)->TryGetStringField(TEXT("memberName"), Member);
        if (!(*Step)->TryGetStringField(TEXT("memberClass"), MemberClass))
        {
            (*Step)->TryGetStringField(TEXT("targetClass"), MemberClass);
        }
        if (Edit == TEXT("add_variable"))
        {
            FString Variable;
            (*Step)->TryGetStringField(TEXT("variableName"), Variable);
            Declared.Add(FName(*Variable));
            continue;
        }
        if (Edit != TEXT("create_node") || Member.IsEmpty())
        {
            continue;
        }
        OutIndex = Index;
        UClass* ResolvedClass = nullptr;
        if (IsCallFunctionType(NodeType) &&
            !ResolveGraphCallFunction(Context.Blueprint, Member, MemberClass, ResolvedClass))
        {
            OutCode = TEXT("FUNCTION_NOT_FOUND");
            UClass* HintClass = ResolvedClass ? ResolvedClass : Context.Blueprint->GeneratedClass.Get();
            return FString::Printf(TEXT("Function '%s' not found.%s"), *Member, *SuggestMemberFix(HintClass, Member));
        }
        const FName Variable(*Member);
        if (IsVariableNodeType(NodeType) && MemberClass.IsEmpty() && !bWidgetBlueprint &&
            !Declared.Contains(Variable) &&
            FBlueprintEditorUtils::FindNewVariableIndex(Context.Blueprint, Variable) == INDEX_NONE &&
            !(Context.Blueprint->GeneratedClass && McpFindPropertyRecursive(Context.Blueprint->GeneratedClass, Variable)))
        {
            OutCode = TEXT("VARIABLE_NOT_FOUND");
            return FString::Printf(TEXT("Variable '%s' not found in the Blueprint, its components or any parent "
                                        "class, and no earlier add_variable step declares it."), *Member);
        }
    }
    OutIndex = INDEX_NONE;
    return FString();
}
}

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

    int32 BadIndex = INDEX_NONE;
    FString BadCode;
    const FString Precheck = PrecheckSteps(Context, *Steps, BadIndex, BadCode);
    if (!Precheck.IsEmpty())
    {
        TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
        Details->SetNumberField(TEXT("succeeded"), 0);
        Details->SetNumberField(TEXT("failedIndex"), BadIndex);
        Context.SendErrorWithDetails(FString::Printf(
            TEXT("build_graph checked every step before running any: operations[%d] would fail. %s "
                 "Nothing was applied."), BadIndex, *Precheck), BadCode, Details);
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

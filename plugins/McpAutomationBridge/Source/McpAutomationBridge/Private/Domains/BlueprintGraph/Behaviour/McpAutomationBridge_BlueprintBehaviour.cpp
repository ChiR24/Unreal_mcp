#include "Domains/BlueprintGraph/Behaviour/McpAutomationBridge_BlueprintBehaviour.h"

#include "Core/Requests/McpResponseCaptureRegistry.h"
#include "Domains/BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersBatchSteps.h"
#include "Editor.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetSaveRegistry.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintAssetLoad.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintCompilation.h"
#include "Foundation/BridgeHelpers/Blueprints/McpAutomationBridgeHelpersBlueprintDiagnostics.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

// Author: check the whole recipe, then remove the previous run's nodes, make the
// hooks, run every step as ONE build_graph batch under a save deferral, compile once,
// and keep the result only when nothing failed and a Blueprint that compiled still does.
namespace McpBlueprintBehaviour
{
namespace
{
FMcpCapturedResponse RunBehaviourBatch(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId,
                                       const TSharedPtr<FMcpBridgeWebSocket>& Socket, UBlueprint* Blueprint,
                                       const Detail::FPlan& Plan)
{
    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("blueprintPath"), Blueprint->GetOutermost()->GetName());
    Payload->SetStringField(TEXT("graphName"), Plan.Page->GetName());
    Payload->SetStringField(TEXT("subAction"), TEXT("build_graph"));
    Payload->SetArrayField(TEXT("operations"), Plan.Ops);
    const FString Id = RequestId + TEXT("#behaviour");
    FMcpResponseCaptureRegistry::Get().Begin(Id);
    McpBlueprintGraphHandlers::FActionContext Context{&Bridge, Id, Payload, Socket, TEXT("build_graph")};
    if (McpBlueprintGraphHandlers::PrepareBlueprintAndGraph(Context))
    {
        McpBlueprintGraphHandlers::GraphBatch::RunGraphBatch(Context, MaxAuthorSteps, /*bCompile=*/false);
    }
    return FMcpResponseCaptureRegistry::Get().End(Id);
}

// The batch's per-step results in the report, plus where the failed step came from.
FString CopyBatchResults(const FMcpCapturedResponse& Reply, const Detail::FPlan& Plan, const TSharedPtr<FJsonObject>& Report)
{
    for (const TCHAR* Field : {TEXT("results"), TEXT("nodeIds"), TEXT("failedIndex"), TEXT("succeeded")})
    {
        if (const TSharedPtr<FJsonValue> Value = Reply.Result.IsValid() ? Reply.Result->TryGetField(Field) : nullptr)
        {
            Report->SetField(Field, Value);
        }
    }
    double Failed = -1;
    if (Reply.Result.IsValid() && Reply.Result->TryGetNumberField(TEXT("failedIndex"), Failed) &&
        Plan.Labels.IsValidIndex(static_cast<int32>(Failed)))
    {
        Report->SetStringField(TEXT("failedStep"), Plan.Labels[static_cast<int32>(Failed)]);
    }
    // The batch stops at its first failing step, so that step is the last result.
    const TArray<TSharedPtr<FJsonValue>>* Results = nullptr;
    const FString StepError = Reply.Result.IsValid() && Reply.Result->TryGetArrayField(TEXT("results"), Results) &&
                                      Results->Num() > 0
                                  ? GetJsonStringField(Results->Last()->AsObject(), TEXT("error"))
                                  : FString();
    return !StepError.IsEmpty() ? StepError
           : Reply.bCaptured    ? Reply.Message
                                : FString(TEXT("the build_graph batch sent no reply"));
}

bool ApplyPlan(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FMcpBridgeWebSocket>& Socket,
               UBlueprint* Blueprint, const FSnapshot& Before, const Detail::FPlan& Plan,
               const TSharedPtr<FJsonObject>& Report, FString& OutError, FString& OutCode)
{
    Report->SetNumberField(TEXT("previousRemoved"), Plan.bReplace ? Detail::RemoveOwned(Blueprint, Plan.Tag) : 0);
    Detail::FWiring Wiring;
    const bool bWired = Detail::WireHooks(Blueprint, Plan, Wiring, OutError, OutCode);
    Report->SetArrayField(TEXT("hooks"), Wiring.Report);
    if (!bWired)
    {
        return false;
    }
    Detail::RewriteRefs(Plan, Wiring);
    Report->SetNumberField(TEXT("stepCount"), Plan.Ops.Num());
    if (Plan.Ops.Num() > 0)
    {
        const FMcpCapturedResponse Reply = RunBehaviourBatch(Bridge, RequestId, Socket, Blueprint, Plan);
        OutError = CopyBatchResults(Reply, Plan, Report);
        if (!Reply.bSuccess)
        {
            OutCode = Reply.ErrorCode.IsEmpty() ? TEXT("STEP_FAILED") : *Reply.ErrorCode;
            return false;
        }
    }
    OutCode = TEXT("DEFAULT_NOT_APPLIED");
    if (!Detail::CommitVariables(Blueprint, Plan, Before, Report, OutError))
    {
        return false;
    }
    FString FirstError;
    const bool bCompiled = McpCompileBlueprintWithDiagnostics(Blueprint, Report, FirstError, 12);
    if (!bCompiled && Before.bCompiled)
    {
        OutCode = TEXT("BEHAVIOUR_BREAKS_COMPILE");
        OutError = FString::Printf(TEXT("the Blueprint compiled before and would not with it: %s"),
                                   FirstError.IsEmpty() ? TEXT("no compiler message") : *FirstError);
        return false;
    }
    Report->SetBoolField(TEXT("preExistingErrors"), !bCompiled);
    TMap<FGuid, FString> SharedTags = Wiring.SharedTags;
    const TSharedPtr<FJsonObject>* NodeIds = nullptr;
    const bool bNodeIds = Report->TryGetObjectField(TEXT("nodeIds"), NodeIds);
    for (const TPair<FString, FString>& Shared : Plan.SharedStepTags)
    {
        FGuid Guid;
        if (bNodeIds && FGuid::Parse(GetJsonStringField(*NodeIds, Shared.Key), Guid))
        {
            SharedTags.Add(Guid, Shared.Value);
        }
    }
    Detail::TagNew(Blueprint, Before, Plan.Tag, SharedTags);
    if (Plan.Input.IsValid() && bNodeIds)
    {
        Plan.Input->SetStringField(TEXT("nodeId"), GetJsonStringField(*NodeIds, Plan.InputId));
    }
    OutCode = TEXT("KEY_MAPPING_FAILED");
    return Detail::EnsureKeyMapping(Bridge, RequestId, Plan, OutError);
}

void ListMembers(const TSharedPtr<FJsonObject>& Report, const Detail::FPlan& Plan)
{
    TArray<TSharedPtr<FJsonValue>> Functions;
    TArray<TSharedPtr<FJsonValue>> Dispatchers;
    TArray<TSharedPtr<FJsonValue>> Events;
    for (const TSharedPtr<FJsonValue>& Op : Plan.Ops)
    {
        const TSharedPtr<FJsonObject> Step = Op->AsObject();
        const FString Edit = GetJsonStringField(Step, TEXT("edit"));
        if (Edit == TEXT("add_function"))
        {
            Functions.Add(MakeShared<FJsonValueString>(GetJsonStringField(Step, TEXT("functionName"))));
        }
        else if (Edit == TEXT("add_event_dispatcher"))
        {
            Dispatchers.Add(MakeShared<FJsonValueString>(GetJsonStringField(Step, TEXT("dispatcherName"))));
        }
    }
    for (const TSharedPtr<FJsonObject>& Event : Plan.CustomEvents)
    {
        Events.Add(MakeShared<FJsonValueString>(GetJsonStringField(Event, TEXT("eventName"))));
    }
    Report->SetArrayField(TEXT("functions"), Functions);
    Report->SetArrayField(TEXT("dispatchers"), Dispatchers);
    Report->SetArrayField(TEXT("customEvents"), Events);
}
} // namespace

FAuthorResult Author(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId,
                     const TSharedPtr<FMcpBridgeWebSocket>& Socket, const FString& BlueprintPath,
                     const TSharedPtr<FJsonObject>& Recipe, const FSnapshot* Before)
{
    FAuthorResult R;
    R.Report = McpHandlerUtils::CreateResultObject();
    const TSharedPtr<FJsonObject> Report = R.Report;
    FString Normalized;
    FString Error = TEXT("Stop Play In Editor first: compiling a Blueprint during PIE reinstances the running game.");
    FString Code = TEXT("PIE_ACTIVE");
    UBlueprint* Blueprint = nullptr;
    if (!(GEditor && GEditor->IsPlaySessionInProgress()))
    {
        Code = Recipe.IsValid() ? TEXT("BLUEPRINT_NOT_FOUND") : TEXT("INVALID_RECIPE");
        Error = TEXT("No recipe was given.");
        Blueprint = Recipe.IsValid() ? LoadBlueprintAsset(BlueprintPath, Normalized, Error) : nullptr;
    }
    Report->SetStringField(TEXT("blueprintPath"), Blueprint ? Blueprint->GetOutermost()->GetName() : BlueprintPath);
    FSnapshot Own;
    if (Blueprint && !Before)
    {
        Own = TakeSnapshot(Blueprint);
    }
    const FSnapshot& Snapshot = Before ? *Before : Own;
    if (Blueprint && (Blueprint->Status == BS_Dirty || Blueprint->Status == BS_Unknown))
    {
        McpSafeCompileBlueprint(Blueprint); // the caller's own edits (Before) must be on the class the checks read
    }
    Detail::FPlan Plan;
    bool bApplied = Blueprint && Detail::BuildPlan(Bridge, RequestId, Blueprint, Recipe, Plan, Report, Error, Code);
    bool bChanged = Before != nullptr;
    if (bApplied)
    {
        FMcpDeferAssetSaves DeferSaves; // no step writes the package; the one save is below
        bChanged = true;
        bApplied = ApplyPlan(Bridge, RequestId, Socket, Blueprint, Snapshot, Plan, Report, Error, Code);
    }
    Report->SetBoolField(TEXT("rolledBack"), false);
    if (!bApplied)
    {
        if (Blueprint && bChanged)
        {
            Detail::Restore(Blueprint, Snapshot, Report);
            // Put the disk back too, whatever a step may have written; a package that was
            // already dirty keeps the caller's unsaved work unsaved.
            Report->SetBoolField(TEXT("saved"), !Snapshot.bPackageDirty && SaveLoadedAssetThrottled(Blueprint, true));
        }
        const FString FailedStep = GetJsonStringField(Report, TEXT("failedStep"));
        const int32 PreviousRemoved = static_cast<int32>(GetJsonNumberField(Report, TEXT("previousRemoved")));
        R.ErrorCode = Code;
        R.Message = FString::Printf(TEXT("Behaviour '%s' was not applied: "), *Plan.Tag);
        R.Message += FailedStep.IsEmpty() ? Error : FailedStep + TEXT(" failed: ") + Error;
        R.Message += !bChanged ? TEXT(" The Blueprint was not changed.") : TEXT(" The Blueprint was put back as it was");
        // ponytail: a replace removes the previous version before the rebuild and cannot bring it back.
        R.Message += !bChanged ? FString()
                     : PreviousRemoved > 0
                         ? FString::Printf(TEXT(", except the %d nodes of this behaviour's previous version, which were "
                                                "removed first; run the action again to rebuild it."), PreviousRemoved)
                         : FString(TEXT("."));
        return R;
    }
    Report->SetBoolField(TEXT("saved"), SaveLoadedAssetThrottled(Blueprint, true));
    ListMembers(Report, Plan);
    R.bApplied = true;
    R.Message = FString::Printf(TEXT("Behaviour '%s' applied to %s: %d steps, %d hooks"), *Plan.Tag, *Blueprint->GetName(),
                                Plan.Ops.Num(), Plan.Hooks.Num());
    R.Message += Plan.Input.IsValid() ? TEXT(", input bound.") : TEXT(".");
    R.Message += GetJsonBoolField(Report, TEXT("compiled")) ? TEXT(" The Blueprint compiles")
                                                            : TEXT(" The Blueprint still has the compile errors it had before");
    R.Message += GetJsonBoolField(Report, TEXT("saved")) ? TEXT(" and was saved.") : TEXT(" and was NOT saved.");
    return R;
}
} // namespace McpBlueprintBehaviour

#include "Domains/PCG/Execution/McpPCGExecution.h"
#if MCP_HAS_PCG_EXECUTION
#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "Foundation/McpScopedEditorTransaction.h"

namespace McpPCGHandlers
{
void FinishPCGOperation(TSharedPtr<FPCGOperation> Op, const FString& State, const FString& Message, const FString& Code)
{
    if (Op->bFinished) return;
    Op->bFinished = true;
    Op->Finished = FPlatformTime::Seconds();
    Op->State = State;
    Op->Message = Message;
    Op->ErrorCode = Code;
    UPCGComponent* Component = Op->Component.Get();
    if (Component && Component->GetPathName() == Op->ComponentPath) Op->Resources = ReadPCGManagedResults(Component);
    if (State == TEXT("succeeded") && Op->bSave && Component)
    {
        FString SaveError;
        if (!SaveEditorWorldIfRequested(Component->GetWorld(), true, Op->bSaved, SaveError))
        {
            Op->State = TEXT("save_failed");
            Op->Message = SaveError;
            Op->ErrorCode = TEXT("SAVE_FAILED");
        }
    }
    if (UEngineWireSubsystem* Bridge = Op->Bridge.Get())
    {
        if (Op->bDeferred) Bridge->ClearAutomationRequestCancellation(Op->RequestId);
        if (Op->bDeferred && !Op->bResponded && !Op->bObservationCancelled.load())
        {
            Op->bResponded = true;
            Bridge->SendProgressUpdate(Op->RequestId, Op->State == TEXT("succeeded") ? 100.0f : -1.0f, Op->Message, false);
            auto Result = DescribePCGOperation(Op);
            FMcpScopedEditorTransaction Undo(FText::FromString(TEXT("PCG Native Operation")), EMcpMutationDurability::AsyncPipeline, TArray<UObject*>());
            Undo.DescribeInto(Result);
            Bridge->SendAutomationResponse(Op->Socket, Op->RequestId, Op->State == TEXT("succeeded"), Op->Message, Result, Op->ErrorCode);
        }
    }
    Op->bResponded = true;
    if (!Component || (!Component->IsGenerating() && !Component->IsCleaningUp())) ReleasePCGOperation(Op);
}

namespace
{
bool AdvanceOperation(const TSharedPtr<FPCGOperation>& Op)
{
    UPCGComponent* Component = Op->Component.Get();
    const bool bBusy = Component && (Component->IsGenerating() || Component->IsCleaningUp());
    if (Op->bFinished)
    {
        if (!bBusy) ReleasePCGOperation(Op);
        return bBusy;
    }
    if (!Component)
    {
        FinishPCGOperation(Op, TEXT("target_gone"), TEXT("The target PCG component disappeared before this operation completed."), TEXT("PCG_TARGET_GONE"));
        return false;
    }
    if (!Component->GetPathName().Equals(Op->ComponentPath, ESearchCase::IgnoreCase))
    {
        FinishPCGOperation(Op, TEXT("target_moved"), TEXT("The target component changed its object path; no new target was selected."), TEXT("PCG_TARGET_MOVED"));
        return bBusy;
    }
    if (!Op->Bridge.IsValid())
    {
        FinishPCGOperation(Op, TEXT("observation_cancelled"), TEXT("The automation subsystem is unavailable; native work was not assumed cancelled."), TEXT("PCG_OBSERVER_UNAVAILABLE"));
        return bBusy;
    }
    if (Op->bObservationCancelled.load())
    {
        FinishPCGOperation(Op, TEXT("observation_cancelled"), TEXT("The request observation was cancelled; native work may still be running."), TEXT("PCG_OBSERVATION_CANCELLED"));
        return bBusy;
    }
    const bool bNativeEnd = Op->bGenerated.load() || Op->bCleaned.load() || Op->bCancelled.load();
    const double Now = FPlatformTime::Seconds();
    if (bNativeEnd || Now - Op->LastInputCheck >= 1.0)
    {
        UPCGGraph* Graph = Component->GetGraph();
        const FString Revision = Graph ? GetPCGGraphRevision(Graph) : TEXT("no_graph");
        const FString InstanceRevision = Op->Kind == TEXT("generate") ? GetPCGInstanceRevision(Component) : FString();
        if (Op->Kind == TEXT("generate") && (Revision.IsEmpty() || InstanceRevision.IsEmpty()))
        {
            FinishPCGOperation(Op, TEXT("input_unavailable"), TEXT("A complete bounded graph or instance revision is unavailable; native work may continue."), TEXT("PCG_SNAPSHOT_UNAVAILABLE"));
            return bBusy;
        }
        if ((Op->Kind == TEXT("generate") && (Graph != Op->Graph.Get() || Revision != Op->GraphRevision || InstanceRevision != Op->InstanceRevision)))
        {
            FinishPCGOperation(Op, TEXT("graph_changed"), TEXT("Graph, component seed or parameter values changed during this operation; no unchanged-version success is claimed."), TEXT("PCG_INPUT_CHANGED"));
            return bBusy;
        }
        // Waiting is not measured execution progress. Repeated heartbeats would consume the
        // WebSocket bridge's bounded progress extensions and shorten its normal request budget.
        Op->LastInputCheck = Now;
    }
    if (Op->GenerationStarts.load() > (Op->Kind == TEXT("generate") ? 1 : 0))
    {
        FinishPCGOperation(Op, TEXT("superseded"), TEXT("Another native generation began on this component; its products are not attributed to this request."), TEXT("PCG_OPERATION_SUPERSEDED"));
        return bBusy;
    }
    if (Op->bCancelled.load())
    {
        int32 ErrorCount = 0;
        Op->Diagnostics = ReadPCGExecutionDiagnostics(Component, Op->bDiagnosticsAvailable, ErrorCount);
        FinishPCGOperation(Op, ErrorCount > 0 ? TEXT("failed") : TEXT("cancelled"),
            ErrorCount > 0 ? TEXT("Native PCG execution aborted with node errors.") : TEXT("Native PCG execution reported cancellation."),
            ErrorCount > 0 ? TEXT("PCG_GENERATION_FAILED") : TEXT("PCG_GENERATION_CANCELLED"));
        return bBusy;
    }
    if ((Op->Kind == TEXT("generate") && Op->bGenerated.load()) || (Op->Kind == TEXT("cleanup") && Op->bCleaned.load()))
    {
        int32 ErrorCount = 0;
        if (Op->Kind == TEXT("generate")) Op->Diagnostics = ReadPCGExecutionDiagnostics(Component, Op->bDiagnosticsAvailable, ErrorCount);
        else Op->bDiagnosticsAvailable = true;
        if (ErrorCount > 0) FinishPCGOperation(Op, TEXT("failed"), TEXT("This native PCG execution completed with node errors."), TEXT("PCG_GENERATION_FAILED"));
        else if (!Op->bDiagnosticsAvailable) FinishPCGOperation(Op, TEXT("unknown"), TEXT("Native completion was observed but execution diagnostics are unavailable."), TEXT("PCG_DIAGNOSTICS_UNAVAILABLE"));
        else FinishPCGOperation(Op, TEXT("succeeded"), Op->Kind == TEXT("generate") ? TEXT("This PCG generation completed without captured node errors.") : TEXT("The component's managed PCG resources were cleaned."));
        return bBusy;
    }
    if (Now - Op->Started >= Op->Timeout)
    {
        FinishPCGOperation(Op, TEXT("timed_out"), TEXT("PCG completion was not observed within the request timeout; this does not cancel native work."), TEXT("PCG_OPERATION_TIMEOUT"));
        return bBusy;
    }
    if (!bBusy && Now - Op->Started > 0.5)
    {
        FinishPCGOperation(Op, TEXT("failed"), TEXT("Native PCG work stopped without a completion or cancellation signal for this request."), TEXT("PCG_COMPLETION_NOT_OBSERVED"));
        return false;
    }
    return true;
}
}

void StartPCGOperationObserver(const TSharedPtr<FPCGOperation>& Op)
{
    if (Op->bFinished || Op->Ticker.IsValid()) return;
    const TWeakPtr<FPCGOperation> Weak = Op;
    Op->Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
    {
        const auto State = Weak.Pin();
        return State.IsValid() && AdvanceOperation(State);
    }), 0.05f);
}
}
#endif

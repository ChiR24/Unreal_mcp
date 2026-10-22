#include "Domains/PCG/Execution/McpPCGExecution.h"
#include "Domains/PCG/Discovery/McpPCGRead.h"
#include "Foundation/BridgeHelpers/Security/McpSubobjectPath.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
bool HandlePCGExecution(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    const TSet<FString> Actions = {TEXT("generate_pcg_graph"), TEXT("cleanup_pcg_component"), TEXT("cancel_pcg_operation"),
        TEXT("get_pcg_operation_status"), TEXT("get_pcg_component_results")};
    if (!Actions.Contains(Action)) return false;
#if !MCP_HAS_PCG_EXECUTION
    Bridge->SendAutomationError(Socket, RequestId, TEXT("The native PCG execution observation APIs are unavailable in this engine version."), TEXT("NOT_SUPPORTED"));
    return true;
#else
    FString Principal;
    if (!ResolvePCGPrincipal(Bridge, RequestId, Socket, Principal))
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("No authenticated request ownership context is available."), TEXT("AUTH_CONTEXT_UNAVAILABLE"));
        return true;
    }
    FString Error;
    if (Action == TEXT("get_pcg_operation_status") || Action == TEXT("cancel_pcg_operation"))
    {
        FString Path, Package;
        const bool bValidPath = McpSubobjectPath::TryCanonicalize(GetJsonStringField(Payload, TEXT("componentPath")), Path, Package);
        auto Op = bValidPath ? FindPCGOperation(GetJsonStringField(Payload, TEXT("operationId")), Principal, Path) : nullptr;
        if (!Op.IsValid())
        {
            Bridge->SendAutomationError(Socket, RequestId, TEXT("No retained operation matches this authenticated principal and component path."), TEXT("PCG_OPERATION_NOT_FOUND"));
            return true;
        }
        if (Action == TEXT("cancel_pcg_operation"))
        {
            if (Op->Kind != TEXT("generate"))
            {
                Bridge->SendAutomationError(Socket, RequestId, TEXT("Native cleanup cancellation is not exposed; cancelling observation never claims that native cleanup stopped."), TEXT("NOT_SUPPORTED"));
                return true;
            }
            if (Op->bWatching)
            {
                UPCGComponent* Component = Op->Component.Get();
                if (!Component || !Component->GetPathName().Equals(Path, ESearchCase::IgnoreCase))
                {
                    Bridge->SendAutomationError(Socket, RequestId, TEXT("The original target is gone or has moved; no replacement component was cancelled."), TEXT("PCG_TARGET_GONE"));
                    return true;
                }
                Op->bCancelRequested = true;
                Component->CancelGeneration();
            }
        }
        Bridge->SendAutomationResponse(Socket, RequestId, true,
            Action == TEXT("cancel_pcg_operation") ? TEXT("Cancellation requested, or the operation already ended. Read native status; cancellation is not a rollback.") : TEXT("PCG operation status read."), DescribePCGOperation(Op));
        return true;
    }
    UPCGComponent* Component = ResolveExactPCGComponent(Payload, Error);
    if (!Component)
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("COMPONENT_NOT_FOUND"));
        return true;
    }
    if (Action == TEXT("get_pcg_component_results"))
    {
        auto Result = ReadPCGManagedResults(Component, GetJsonIntField(Payload, TEXT("offset"), 0),
            GetJsonIntField(Payload, TEXT("limit"), 50), GetJsonIntField(Payload, TEXT("sampleLimit"), 2));
        if (!Result.IsValid())
        {
            Bridge->SendAutomationError(Socket, RequestId, TEXT("A complete bounded managed inventory is unavailable; narrow the page or wait for accessible resources."), TEXT("PCG_SNAPSHOT_UNAVAILABLE"));
            return true;
        }
        Result->SetStringField(TEXT("snapshotKind"), TEXT("current_managed_inventory_not_a_completion_signal"));
        if (auto Active = FindActivePCGOperation(Component, Principal)) Result->SetStringField(TEXT("activeOperationId"), Active->Id);
        Bridge->SendAutomationResponse(Socket, RequestId, true, TEXT("PCG managed resources and bounded output summaries read."), Result);
        return true;
    }
    if (GEditor && GEditor->IsTransactionActive())
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Finish the active editor transaction before scheduling native PCG work."), TEXT("EDITOR_BUSY"));
        return true;
    }
    const bool bGenerate = Action == TEXT("generate_pcg_graph");
    if (bGenerate && !Component->GetGraph())
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("The component has no assigned PCG graph."), TEXT("GRAPH_NOT_ASSIGNED"));
        return true;
    }
    if (Component->IsPartitioned() || Component->IsManagedByRuntimeGenSystem())
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("This bounded editor execution flow targets unpartitioned PCG components; partitioned/runtime generation is not claimed."), TEXT("NOT_SUPPORTED"));
        return true;
    }
    if (Component->GetGraph() && !CheckPCGGraphRevision(Component->GetGraph(), Payload, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("STALE_STATE"));
        return true;
    }
    auto Op = BeginPCGOperation(Bridge, RequestId, Socket, Component, bGenerate ? TEXT("generate") : TEXT("cleanup"), true,
        GetJsonBoolField(Payload, TEXT("save"), false), GetJsonNumberField(Payload, TEXT("timeoutSeconds"), 60), Error);
    if (!Op.IsValid())
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("PCG_COMPONENT_BUSY"));
        return true;
    }
    Component->Modify();
    if (bGenerate)
    {
        ClearPCGExecutionDiagnostics(Component);
        Op->TaskId = Component->GenerateLocalGetTaskId(GetJsonBoolField(Payload, TEXT("force"), true));
    }
    else Op->TaskId = Component->CleanupLocal(true, TArray<FPCGTaskId>());
    if (Op->TaskId == InvalidPCGTaskId && !(bGenerate ? Op->bGenerated.load() : Op->bCleaned.load()))
    {
        if (!bGenerate)
        {
            auto Remaining = ReadPCGManagedResults(Component, 0, 1, 0);
            if (Remaining.IsValid() && Remaining->GetNumberField(TEXT("managedResourceCount")) == 0 && Remaining->GetNumberField(TEXT("outputDataCount")) == 0)
            {
                Op->bNoOp = true;
                Op->bDiagnosticsAvailable = true;
                FinishPCGOperation(Op, TEXT("succeeded"), TEXT("No managed PCG resources or outputs remain; native cleanup did not need scheduling."));
                return true;
            }
        }
        FinishPCGOperation(Op, TEXT("failed"), bGenerate ? TEXT("This request did not schedule generation; old generated state is not a success signal.") : TEXT("Native cleanup was not scheduled."),
            bGenerate ? TEXT("GENERATION_NOT_SCHEDULED") : TEXT("CLEANUP_NOT_SCHEDULED"));
        return true;
    }
    StartPCGOperationObserver(Op);
    return true;
#endif
}
}
#endif

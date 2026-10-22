#include "Domains/PCG/Execution/McpPCGExecution.h"
#if MCP_HAS_PCG_EXECUTION
#include "Domains/PCG/Discovery/McpPCGRead.h"

namespace McpPCGHandlers
{
namespace
{
TMap<FString, TSharedPtr<FPCGOperation>> Operations;
TMap<TWeakObjectPtr<UPCGComponent>, FString> ActiveOperations;
constexpr int32 MaxActive = 64, MaxCompleted = 128;
constexpr double CompletedTtl = 1800.0;

void PurgeCompleted()
{
    const double Now = FPlatformTime::Seconds();
    TArray<TSharedPtr<FPCGOperation>> Completed;
    for (auto It = Operations.CreateIterator(); It; ++It)
    {
        const auto& Op = It.Value();
        if (Op->bWatching || !Op->bFinished) continue;
        if (Now - Op->Finished > CompletedTtl) It.RemoveCurrent();
        else Completed.Add(Op);
    }
    Completed.Sort([](const TSharedPtr<FPCGOperation>& A, const TSharedPtr<FPCGOperation>& B) { return A->Finished < B->Finished; });
    for (int32 Index = 0; Index < Completed.Num() - MaxCompleted; ++Index) Operations.Remove(Completed[Index]->Id);
}
}

bool HasActivePCGOperation(UPCGComponent* Component)
{
    return ActiveOperations.Contains(TWeakObjectPtr<UPCGComponent>(Component));
}

TSharedPtr<FPCGOperation> FindActivePCGOperation(UPCGComponent* Component, const FString& Principal)
{
    const FString* Id = ActiveOperations.Find(TWeakObjectPtr<UPCGComponent>(Component));
    return Id ? FindPCGOperation(*Id, Principal, Component->GetPathName()) : nullptr;
}

TSharedPtr<FPCGOperation> FindPCGOperation(const FString& Id, const FString& Principal, const FString& ComponentPath)
{
    PurgeCompleted();
    const auto* Found = Operations.Find(Id);
    return Found && (*Found)->Principal == Principal && (*Found)->ComponentPath.Equals(ComponentPath, ESearchCase::IgnoreCase) ? *Found : nullptr;
}

TSharedPtr<FPCGOperation> BeginPCGOperation(UEngineWireSubsystem* Bridge, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket, UPCGComponent* Component, const FString& Kind, bool bDeferred, bool bSave,
    double Timeout, FString& OutError)
{
    PurgeCompleted();
    if (!Component || Component->IsGenerating() || Component->IsCleaningUp() || HasActivePCGOperation(Component) || ActiveOperations.Num() >= MaxActive)
    {
        OutError = TEXT("The PCG component is busy or the bounded operation capacity is full.");
        return nullptr;
    }
    FString Principal;
    if (!ResolvePCGPrincipal(Bridge, RequestId, Socket, Principal))
    {
        OutError = TEXT("No trusted authenticated request principal is available for PCG operation ownership.");
        return nullptr;
    }
    auto Op = MakeShared<FPCGOperation>();
    Op->Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    Op->RequestId = RequestId;
    Op->Principal = Principal;
    Op->Bridge = Bridge;
    Op->Socket = Socket;
    Op->Component = Component;
    Op->ComponentPath = Component->GetPathName();
    Op->Graph = Component->GetGraph();
    Op->GraphPath = Component->GetGraph() ? Component->GetGraph()->GetPathName() : FString();
    Op->GraphRevision = Component->GetGraph() ? GetPCGGraphRevision(Component->GetGraph()) : TEXT("no_graph");
    Op->InstanceRevision = GetPCGInstanceRevision(Component);
    if (Op->GraphRevision.IsEmpty() || Op->InstanceRevision.IsEmpty())
    {
        OutError = TEXT("A complete graph and instance revision could not be captured.");
        return nullptr;
    }
    Op->Kind = Kind;
    Op->bDeferred = bDeferred;
    Op->bSave = bSave;
    Op->Timeout = FMath::Clamp(Timeout, 0.1, 600.0);
    const TWeakPtr<FPCGOperation> Weak = Op;
    Op->GeneratedHandle = Component->OnPCGGraphGeneratedDelegate.AddLambda([Weak](UPCGComponent*) { if (auto State = Weak.Pin()) State->bGenerated.store(true); });
    Op->CleanedHandle = Component->OnPCGGraphCleanedDelegate.AddLambda([Weak](UPCGComponent*) { if (auto State = Weak.Pin()) State->bCleaned.store(true); });
    Op->CancelledHandle = Component->OnPCGGraphCancelledDelegate.AddLambda([Weak](UPCGComponent*) { if (auto State = Weak.Pin()) State->bCancelled.store(true); });
    Op->StartedHandle = Component->OnPCGGraphStartGeneratingDelegate.AddLambda([Weak](UPCGComponent*) { if (auto State = Weak.Pin()) State->GenerationStarts.fetch_add(1); });
    Operations.Add(Op->Id, Op);
    ActiveOperations.Add(Op->Component, Op->Id);
    if (bDeferred) Bridge->RegisterAutomationRequestCancellation(RequestId, [Weak] { if (auto State = Weak.Pin()) State->bObservationCancelled.store(true); });
    return Op;
}

void ReleasePCGOperation(const TSharedPtr<FPCGOperation>& Op)
{
    if (!Op->bWatching) return;
    Op->bWatching = false;
    if (UPCGComponent* Component = Op->Component.Get())
    {
        Component->OnPCGGraphGeneratedDelegate.Remove(Op->GeneratedHandle);
        Component->OnPCGGraphCleanedDelegate.Remove(Op->CleanedHandle);
        Component->OnPCGGraphCancelledDelegate.Remove(Op->CancelledHandle);
        Component->OnPCGGraphStartGeneratingDelegate.Remove(Op->StartedHandle);
    }
    if (const FString* Current = ActiveOperations.Find(Op->Component))
    {
        if (*Current == Op->Id) ActiveOperations.Remove(Op->Component);
    }
    if (Op->Ticker.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(Op->Ticker);
        Op->Ticker.Reset();
    }
    Op->Socket.Reset();
    PurgeCompleted();
}

void ShutdownPCGOperations(UEngineWireSubsystem* Bridge)
{
    check(IsInGameThread());
    TArray<TSharedPtr<FPCGOperation>> Owned;
    for (const auto& Entry : Operations)
        if (!Bridge || Entry.Value->Bridge.Get() == Bridge) Owned.Add(Entry.Value);
    for (const auto& Op : Owned)
    {
        if (UEngineWireSubsystem* Owner = Op->Bridge.Get())
            if (Op->bDeferred) Owner->ClearAutomationRequestCancellation(Op->RequestId);
        Op->bObservationCancelled.store(true);
        Op->bFinished = true;
        Op->bResponded = true;
        Op->Finished = FPlatformTime::Seconds();
        Op->State = TEXT("observation_cancelled");
        Op->ErrorCode = TEXT("PCG_OBSERVER_UNAVAILABLE");
        Op->Message = TEXT("PCG observation ended with subsystem shutdown; native work was not cancelled.");
        ReleasePCGOperation(Op);
        Op->Bridge.Reset();
        Operations.Remove(Op->Id);
    }
}

TSharedPtr<FJsonObject> DescribePCGOperation(const TSharedPtr<FPCGOperation>& Op)
{
    auto Result = McpHandlerUtils::CreateResultObject();
    UPCGComponent* Component = Op->Component.Get();
    const bool bRunning = Op->bWatching && Component && (Component->IsGenerating() || Component->IsCleaningUp());
    Result->SetStringField(TEXT("operationId"), Op->Id);
    Result->SetStringField(TEXT("kind"), Op->Kind);
    Result->SetStringField(TEXT("state"), Op->State);
    Result->SetStringField(TEXT("componentPath"), Op->ComponentPath);
    Result->SetStringField(TEXT("graphPath"), Op->GraphPath);
    Result->SetStringField(TEXT("graphRevision"), Op->GraphRevision);
    Result->SetStringField(TEXT("instanceRevision"), Op->InstanceRevision);
    if (Op->TaskId != InvalidPCGTaskId) Result->SetNumberField(TEXT("taskId"), static_cast<double>(Op->TaskId));
    Result->SetBoolField(TEXT("completed"), Op->bFinished);
    Result->SetBoolField(TEXT("noOp"), Op->bNoOp);
    Result->SetBoolField(TEXT("succeeded"), Op->State == TEXT("succeeded"));
    Result->SetBoolField(TEXT("engineStillRunning"), bRunning);
    Result->SetBoolField(TEXT("cancellationRequested"), Op->bCancelRequested);
    Result->SetBoolField(TEXT("observationCancelled"), Op->bObservationCancelled.load());
    Result->SetBoolField(TEXT("nativeGenerated"), Op->bGenerated.load());
    Result->SetBoolField(TEXT("nativeCleaned"), Op->bCleaned.load());
    Result->SetBoolField(TEXT("nativeCancelled"), Op->bCancelled.load());
    Result->SetBoolField(TEXT("lateCompletionObserved"), Op->bFinished && Op->State != TEXT("succeeded") && (Op->bGenerated.load() || Op->bCleaned.load()));
    Result->SetBoolField(TEXT("diagnosticsAvailable"), Op->bDiagnosticsAvailable);
    Result->SetBoolField(TEXT("saved"), Op->bSaved);
    Result->SetNumberField(TEXT("elapsedSeconds"), (Op->bFinished ? Op->Finished : FPlatformTime::Seconds()) - Op->Started);
    Result->SetStringField(TEXT("message"), Op->Message);
    if (!Op->ErrorCode.IsEmpty()) Result->SetStringField(TEXT("operationErrorCode"), Op->ErrorCode);
    Result->SetArrayField(TEXT("diagnostics"), Op->Diagnostics);
    if (Op->Resources.IsValid()) Result->SetObjectField(TEXT("resources"), Op->Resources);
    Result->SetBoolField(TEXT("resourceSnapshotFromSuccessfulOperation"), Op->State == TEXT("succeeded") && Op->Resources.IsValid());
    Result->SetStringField(TEXT("retention"), TEXT("current_process_128_completed_30_minutes"));
    return Result;
}
}
#endif

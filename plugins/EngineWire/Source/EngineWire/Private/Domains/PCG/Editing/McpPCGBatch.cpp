#include "Domains/PCG/Editing/McpPCGMutations.h"
#include "Domains/PCG/Editor/McpPCGEditor.h"
#include "Foundation/McpScopedEditorTransaction.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
namespace
{
bool RunOperations(UPCGGraph* Graph, const TArray<TSharedPtr<FJsonValue>>& Operations, FPCGMutationContext& Context,
    TArray<TSharedPtr<FJsonValue>>& Results, FString& OutError, FString& OutCode, int32& OutFailedIndex)
{
    for (int32 Index = 0; Index < Operations.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Operation = nullptr;
        TSharedPtr<FJsonObject> Result;
        OutFailedIndex = Index;
        if (!Operations[Index].IsValid() || !Operations[Index]->TryGetObject(Operation) || !Operation || !Operation->IsValid())
        {
            OutError = TEXT("Each PCG batch step must be an object.");
            OutCode = TEXT("INVALID_ARGUMENT");
            return false;
        }
        if (!ApplyPCGMutation(Graph, *Operation, Context, Result, OutError, OutCode)) return false;
        Results.Add(MakeShared<FJsonValueObject>(Result));
    }
    OutFailedIndex = INDEX_NONE;
    return true;
}
}

bool HandlePCGEditOperations(UEngineWireSubsystem* Bridge, const FString& RequestId, const FString& Action,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph, const FString& GraphPath)
{
    const bool bBatch = Action == TEXT("batch_edit_pcg_graph") || Action == TEXT("batch_edit_pcg_graph_destructive");
    const bool bDestructive = Action == TEXT("batch_edit_pcg_graph_destructive") || Action == TEXT("disconnect_pcg_pins") || Action == TEXT("remove_pcg_node");
    TArray<TSharedPtr<FJsonValue>> Operations;
    if (bBatch)
    {
        const TArray<TSharedPtr<FJsonValue>>* Requested = nullptr;
        FString Expected;
        if (!Payload->TryGetArrayField(TEXT("operations"), Requested) || !Requested || Requested->IsEmpty() || Requested->Num() > 100 ||
            !Payload->TryGetStringField(TEXT("expectedGraphRevision"), Expected) || Expected.IsEmpty())
        {
            Bridge->SendAutomationError(Socket, RequestId, TEXT("A batch requires 1-100 operations and the nonempty expectedGraphRevision read from this graph."), TEXT("INVALID_ARGUMENT"));
            return true;
        }
        Operations = *Requested;
    }
    else
    {
        const FString Op = Action == TEXT("move_pcg_node") ? TEXT("move_node") : Action == TEXT("remove_pcg_node") ? TEXT("remove_node") : TEXT("disconnect");
        auto Operation = MakeShared<FJsonObject>();
        Operation->Values = Payload->Values;
        for (const TCHAR* Field : {TEXT("action"), TEXT("subAction"), TEXT("graphPath"), TEXT("assetPath"), TEXT("save"), TEXT("expectedGraphRevision"), TEXT("edit"), TEXT("mutation")}) Operation->RemoveField(Field);
        Operation->SetStringField(TEXT("op"), Op);
        Operations.Add(MakeShared<FJsonValueObject>(Operation));
    }
    FString Error, Code;
    if (!CheckPCGGraphRevision(Graph, Payload, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("STALE_STATE"));
        return true;
    }
    if (GEditor && GEditor->IsTransactionActive())
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("An existing editor transaction must finish before atomic PCG editing."), TEXT("EDITOR_BUSY"));
        return true;
    }
    TStrongObjectPtr<UPCGGraph> Staged(DuplicateObject<UPCGGraph>(Graph, GetTransientPackage()));
    if (!Staged.IsValid())
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not stage the PCG graph."), TEXT("SNAPSHOT_FAILED"));
        return true;
    }
    Staged->SetFlags(RF_Transient);
    FPCGMutationContext Preflight;
    Preflight.bDestructive = bDestructive;
    TArray<TSharedPtr<FJsonValue>> StagedResults;
    int32 FailedIndex = INDEX_NONE;
    const bool bPreflightApplied = RunOperations(Staged.Get(), Operations, Preflight, StagedResults, Error, Code, FailedIndex);
    if (bPreflightApplied && GetPCGGraphRevision(Staged.Get()).IsEmpty())
    {
        Error = TEXT("The edited graph exceeds the authored read budget or has invalid dependencies.");
        Code = TEXT("PCG_SNAPSHOT_UNAVAILABLE");
    }
    if (!bPreflightApplied || Code == TEXT("PCG_SNAPSHOT_UNAVAILABLE"))
    {
        auto Result = McpHandlerUtils::CreateResultObject();
        Result->SetBoolField(TEXT("committed"), false);
        Result->SetBoolField(TEXT("saved"), false);
        Result->SetBoolField(TEXT("rolledBack"), false);
        Result->SetNumberField(TEXT("failedIndex"), FailedIndex);
        Result->SetStringField(TEXT("phase"), TEXT("preflight"));
        Bridge->SendAutomationResponse(Socket, RequestId, false, Error, Result, Code);
        return true;
    }
    FPCGEditState State(Graph);
    if (State.GetRevisionBefore().IsEmpty())
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Could not snapshot authored PCG state."), TEXT("SNAPSHOT_FAILED"));
        return true;
    }
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Edit PCG Graph")),
        bSave ? EMcpMutationDurability::DurablePackageWrite : EMcpMutationDurability::EditorStateOnly, State.GetObjects());
    FPCGMutationContext Context;
    Context.bDestructive = bDestructive;
    TArray<TSharedPtr<FJsonValue>> Results;
    TSharedPtr<FJsonObject> EditorState;
    auto Result = McpHandlerUtils::CreateResultObject();
    const bool bApplied = RunOperations(Graph, Operations, Context, Results, Error, Code, FailedIndex);
    const bool bSynced = bApplied && SyncPCGEditorGraph(Graph, EditorState, Error);
    if (!bApplied || !bSynced)
    {
        const bool bRestored = State.Restore();
        Result->SetBoolField(TEXT("committed"), false);
        Result->SetBoolField(TEXT("saved"), false);
        Result->SetBoolField(TEXT("rolledBack"), bRestored);
        Result->SetBoolField(TEXT("residualChanges"), !bRestored);
        Result->SetNumberField(TEXT("failedIndex"), FailedIndex);
        Result->SetStringField(TEXT("phase"), TEXT("commit"));
        Bridge->SendAutomationResponse(Socket, RequestId, false, Error, Result,
            bRestored ? (bApplied ? TEXT("EDITOR_SYNC_FAILED") : Code) : TEXT("ROLLBACK_FAILED"));
        return true;
    }
    Result->SetStringField(TEXT("graphPath"), GraphPath);
    Result->SetStringField(TEXT("previousRevision"), State.GetRevisionBefore());
    Result->SetStringField(TEXT("revision"), GetPCGGraphRevision(Graph));
    Result->SetBoolField(TEXT("committed"), true);
    Result->SetBoolField(TEXT("rolledBack"), false);
    Result->SetArrayField(TEXT("results"), Results);
    Result->SetObjectField(TEXT("editorGraph"), EditorState);
    auto Temporary = MakeShared<FJsonObject>();
    for (const auto& Pair : Context.TemporaryNodes) Temporary->SetStringField(Pair.Key, Pair.Value);
    Result->SetObjectField(TEXT("nodeIds"), Temporary);
    Transaction.DescribeInto(Result);
    bool bSaved = false;
    const bool bSuccess = SaveGraphIfRequested(Graph, bSave, bSaved, Error);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Bridge->SendAutomationResponse(Socket, RequestId, bSuccess, bSuccess ? TEXT("PCG graph edits committed.") : Error, Result,
        bSuccess ? FString() : TEXT("SAVE_FAILED"));
    return true;
}
}
#endif

#include "Domains/PCG/Editor/McpPCGEditor.h"
#include "Domains/PCG/Editing/McpPCGEditState.h"
#include "Foundation/McpScopedEditorTransaction.h"
#include "Subsystems/AssetEditorSubsystem.h"

#if MCP_HAS_PCG
namespace McpPCGHandlers
{
bool HandleSyncPCGEditor(UEngineWireSubsystem* Bridge, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket, bool bSave, UPCGGraph* Graph, const FString& GraphPath)
{
    FString Error;
    if (!CheckPCGGraphRevision(Graph, Payload, Error))
    {
        Bridge->SendAutomationError(Socket, RequestId, Error, TEXT("STALE_STATE"));
        return true;
    }
#if !MCP_HAS_PCG_EDITOR
    Bridge->SendAutomationError(Socket, RequestId, TEXT("Public PCG editor synchronization is unavailable in this build."), TEXT("NOT_SUPPORTED"));
    return true;
#else
    bool bAmbiguous = false;
    UEdGraph* Editor = FindExistingPCGEditorGraph(Graph, bAmbiguous);
    if (bAmbiguous)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Ambiguous owned PCG editor graph."), TEXT("EDITOR_GRAPH_MISMATCH"));
        return true;
    }
    // Explicit synchronization is a write. Ordinary read and edit actions never open a tab.
    const bool bOpenEditor = GetJsonBoolField(Payload, TEXT("openEditor"), false);
    if (!Editor && bOpenEditor && GEditor)
    {
        UAssetEditorSubsystem* Editors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if (!Editors || !Editors->OpenEditorForAsset(Graph))
        {
            Bridge->SendAutomationError(Socket, RequestId, TEXT("The PCG graph editor could not be opened."), TEXT("EDITOR_OPEN_FAILED"));
            return true;
        }
    }
    FPCGEditState State(Graph);
    FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Synchronize PCG Editor")),
        bSave ? EMcpMutationDurability::DurablePackageWrite : EMcpMutationDurability::EditorStateOnly, State.GetObjects());
    TSharedPtr<FJsonObject> Result;
    if (!SyncPCGEditorGraph(Graph, Result, Error))
    {
        const bool bRestored = State.Restore();
        Result->SetBoolField(TEXT("rolledBack"), bRestored);
        Result->SetBoolField(TEXT("committed"), false);
        Result->SetBoolField(TEXT("saved"), false);
        Bridge->SendAutomationResponse(Socket, RequestId, false, Error, Result, bRestored ? TEXT("EDITOR_SYNC_FAILED") : TEXT("ROLLBACK_FAILED"));
        return true;
    }
    Result->SetStringField(TEXT("graphPath"), GraphPath);
    Result->SetStringField(TEXT("revision"), GetPCGGraphRevision(Graph));
    Result->SetBoolField(TEXT("committed"), true);
    Result->SetBoolField(TEXT("rolledBack"), false);
    Transaction.DescribeInto(Result);
    bool bSaved = false;
    const bool bSuccess = SaveGraphIfRequested(Graph, bSave, bSaved, Error);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Bridge->SendAutomationResponse(Socket, RequestId, bSuccess,
        bSuccess ? TEXT("PCG editor representation synchronized or not yet created.") : Error, Result, bSuccess ? FString() : TEXT("SAVE_FAILED"));
    return true;
#endif
}
}
#endif

#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

#if WITH_EDITOR
#include "Editor/Transactor.h"

namespace {
// These handlers used to Exec the console text "Undo" / "Redo", which is not an
// editor command (UNDO and REDO are TRANSACTION subcommands), so every call
// answered "Undo executed" while nothing was undone. Drive the transactor
// directly, name the transaction that moved, and say when there is none.
void RunUndoRedo(UMcpAutomationBridgeSubsystem *Bridge, const FString &RequestId,
                 TSharedPtr<FMcpBridgeWebSocket> Socket, bool bUndo) {
  UTransactor *Trans = GEditor ? GEditor->Trans.Get() : nullptr;
  FText Reason;
  if (!Trans || !(bUndo ? Trans->CanUndo(&Reason) : Trans->CanRedo(&Reason))) {
    const FString Fallback = bUndo ? TEXT("There is no editor transaction to undo.")
                                   : TEXT("There is no undone transaction to redo.");
    SendStandardErrorResponse(Bridge, Socket, RequestId, bUndo ? TEXT("NOTHING_TO_UNDO") : TEXT("NOTHING_TO_REDO"),
                              Reason.IsEmpty() ? Fallback : Reason.ToString(), nullptr);
    return;
  }
  const FString Title = (bUndo ? Trans->GetUndoContext() : Trans->GetRedoContext()).Title.ToString();
  const bool bDone = bUndo ? GEditor->UndoTransaction() : GEditor->RedoTransaction();
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("command"), bUndo ? TEXT("Undo") : TEXT("Redo"));
  Resp->SetStringField(TEXT("transaction"), Title);
  if (!bDone) {
    SendStandardErrorResponse(Bridge, Socket, RequestId, bUndo ? TEXT("UNDO_FAILED") : TEXT("REDO_FAILED"),
                              FString::Printf(TEXT("The editor refused to %s '%s'."), bUndo ? TEXT("undo") : TEXT("redo"), *Title),
                              Resp);
    return;
  }
  Bridge->SendAutomationResponse(Socket, RequestId, true,
                                 FString::Printf(TEXT("%s: %s"), bUndo ? TEXT("Undid") : TEXT("Redid"), *Title), Resp,
                                 FString());
}
} // namespace
#endif

bool UMcpAutomationBridgeSubsystem::HandleControlEditorUndo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  RunUndoRedo(this, RequestId, Socket, true);
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorRedo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  RunUndoRedo(this, RequestId, Socket, false);
  return true;
#else
  return false;
#endif
}

#include "McpAutomationBridge_ControlEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlEditorUndo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId,
                              TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("Undo"));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("command"), TEXT("Undo"));
  SendAutomationResponse(Socket, RequestId, true, TEXT("Undo executed"), Resp,
                         FString());
  return true;
#else
  return false;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorRedo(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId,
                              TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  GEditor->Exec(GEditor->GetEditorWorldContext().World(), TEXT("Redo"));

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("command"), TEXT("Redo"));
  SendAutomationResponse(Socket, RequestId, true, TEXT("Redo executed"), Resp,
                         FString());
  return true;
#else
  return false;
#endif
}

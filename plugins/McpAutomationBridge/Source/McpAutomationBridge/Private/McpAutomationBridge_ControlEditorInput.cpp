#include "McpAutomationBridge_ControlEditorSupport.h"

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSimulateInput(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!GEditor) {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"),
                              TEXT("Editor not available"), nullptr);
    return true;
  }

  const FString InputType = NormalizeSimulatedInputTypeForMcp(Payload);
  FString Key;
  Payload->TryGetStringField(TEXT("key"), Key);

  bool bSuccess = false;
  bool bRoutedToPIE = false;
  bool bHandledByPIE = false;
  bool bHandledBySlate = false;
  FString Message;

  SimulateEditorInputForMcp(InputType, Key, Payload, bSuccess, bRoutedToPIE,
                            bHandledByPIE, bHandledBySlate, Message);

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bSuccess);
  Resp->SetStringField(TEXT("type"), InputType);
  Resp->SetStringField(TEXT("message"), Message);
  Resp->SetBoolField(TEXT("routedToPIE"), bRoutedToPIE);
  Resp->SetBoolField(TEXT("handledByPIE"), bHandledByPIE);
  Resp->SetBoolField(TEXT("handledBySlate"), bHandledBySlate);
  AddSimulatedInputDiagnosticsForMcp(Key, Resp);

  if (bSuccess) {
    SendAutomationResponse(Socket, RequestId, true, Message, Resp, FString());
  } else {
    SendStandardErrorResponse(this, Socket, RequestId, TEXT("INPUT_FAILED"),
                              Message, Resp);
  }
  return true;
#else
  SendStandardErrorResponse(this, Socket, RequestId, TEXT("NOT_IMPLEMENTED"),
                            TEXT("Simulate input requires editor build."), nullptr);
  return true;
#endif
}

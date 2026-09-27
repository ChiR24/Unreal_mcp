#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Ui/McpAutomationBridge_UiHandlersPrivate.h"

#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

bool UMcpAutomationBridgeSubsystem::HandleUiAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString LowerAction = Action.ToLower();
  const bool bIsSystemControl =
      LowerAction.Equals(TEXT("system_control"), ESearchCase::IgnoreCase);
  if (!bIsSystemControl) {
    return false;
  }

  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId, TEXT("Payload missing."),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString SubAction;
  if (Payload->HasField(TEXT("subAction"))) {
    SubAction = GetJsonStringField(Payload, TEXT("subAction"));
  } else {
    Payload->TryGetStringField(TEXT("action"), SubAction);
  }
  const FString LowerSub = SubAction.ToLower();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetStringField(TEXT("action"), LowerSub);

  bool bSuccess = false;
  bool bHandled = false;
  bool bResponseSent = false;
  FString Message;
  FString ErrorCode;

  if (McpUiHandlers::HandleSystemExtrasAction(*this, RequestId, LowerSub, Payload,
                                              RequestingSocket)) {
    return true;
  }

  const McpUiHandlers::FUiScreenshotFallback ScreenshotFallback =
      [this, &RequestId, &Payload, &RequestingSocket]() {
        return HandleControlEditorScreenshot(RequestId, Payload,
                                             RequestingSocket);
      };

  bHandled =
      McpUiHandlers::HandleWidgetAuthoringAction(*this, LowerSub, Payload,
                                                 Resp, bSuccess, Message,
                                                 ErrorCode) ||
      McpUiHandlers::HandleScreenshotAction(
          *this, RequestId, bIsSystemControl, LowerSub, Payload,
          RequestingSocket, Resp, bSuccess, Message, ErrorCode, bResponseSent,
          ScreenshotFallback) ||
      McpUiHandlers::HandleProjectSettingsAction(LowerSub, Payload, Resp,
                                                 bSuccess, Message, ErrorCode);

  if (bResponseSent) {
    return true;
  }

  if (!bHandled) {
    Message = FString::Printf(
        TEXT("System control action '%s' not implemented"), *LowerSub);
    ErrorCode = TEXT("NOT_IMPLEMENTED");
    Resp->SetStringField(TEXT("error"), Message);
  }

  Resp->SetBoolField(TEXT("success"), bSuccess);
  if (Message.IsEmpty()) {
    Message = bSuccess ? TEXT("System control action completed")
                       : TEXT("System control action failed");
  }

  SendAutomationResponse(RequestingSocket, RequestId, bSuccess, Message, Resp,
                         ErrorCode);
  return true;
}

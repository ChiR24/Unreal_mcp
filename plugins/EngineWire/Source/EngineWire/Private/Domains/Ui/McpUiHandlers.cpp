#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Ui/McpUiPrivate.h"

#include "Foundation/BridgeHelpers/Responses/McpJsonFields.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

bool UEngineWireSubsystem::HandleUiAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  // Both callers, system_control's sub-route and control_editor's game_viewport screenshot, pass a payload whose
  // subAction names the action.
  const FString LowerSub = GetJsonStringField(Payload, TEXT("subAction")).ToLower();

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
          *this, RequestId, LowerSub, Payload,
          RequestingSocket, Resp, bSuccess, Message, ErrorCode, bResponseSent,
          ScreenshotFallback) ||
      McpUiHandlers::HandleProjectSettingsAction(LowerSub, Payload, Resp,
                                                 bSuccess, Message, ErrorCode) ||
      McpUiHandlers::HandleProjectThumbnailAction(LowerSub, Payload, Resp,
                                                  bSuccess, Message, ErrorCode) ||
      McpUiHandlers::HandleRenderSettingsAction(LowerSub, Payload, Resp,
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

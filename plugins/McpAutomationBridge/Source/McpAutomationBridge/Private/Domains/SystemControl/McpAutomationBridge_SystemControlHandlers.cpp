#include "Domains/SystemControl/McpAutomationBridge_SystemControlHandlersPrivate.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"

bool UMcpAutomationBridgeSubsystem::HandleSystemControlAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  FString SubAction;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("action"), SubAction);
  }

  const FString Lower = SubAction.ToLower();
  const bool bInsightsAction =
      Lower == TEXT("start_session") ||
      Lower == TEXT("start_unreal_insights") ||
      Lower == TEXT("capture_insights_trace") ||
      Lower == TEXT("get_trace_status") ||
      Lower == TEXT("pause_session") ||
      Lower == TEXT("resume_session") ||
      Lower == TEXT("stop_session") ||
      Lower == TEXT("write_snapshot") ||
      Lower == TEXT("send_snapshot") ||
      Lower == TEXT("analyze_trace");
  const bool bLogSubscriptionAction =
      Lower == TEXT("subscribe") || Lower == TEXT("unsubscribe");
  if (!Lower.StartsWith(TEXT("run_ubt")) &&
      !Lower.StartsWith(TEXT("run_tests")) &&
      Lower != TEXT("cancel_tests") &&
      !Lower.StartsWith(TEXT("test_progress")) &&
      !Lower.StartsWith(TEXT("test_stale")) &&
      Lower != TEXT("export_asset") &&
      !bInsightsAction &&
      !bLogSubscriptionAction &&
      Lower != TEXT("validate_assets") &&
      Lower != TEXT("execute_python")) {
    return false;
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("System control payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  if (bInsightsAction) {
    return HandleInsightsAction(RequestId, TEXT("manage_insights"), Payload,
                                RequestingSocket);
  }
  if (bLogSubscriptionAction) {
    return HandleLogAction(RequestId, TEXT("manage_logs"), Payload,
                           RequestingSocket);
  }
  if (Lower == TEXT("validate_assets")) {
    return McpSystemControlHandlers::HandleValidateAssets(
        this, RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("run_ubt")) {
    return McpSystemControlHandlers::HandleRunUbt(this, RequestId, Payload,
                                                  RequestingSocket);
  }
  if (Lower == TEXT("run_tests")) {
    return McpSystemControlHandlers::HandleRunTests(this, RequestId, Payload,
                                                    RequestingSocket);
  }
  if (Lower == TEXT("cancel_tests")) {
    return McpSystemControlHandlers::HandleCancelTests(
        this, RequestId, RequestingSocket);
  }
  if (Lower == TEXT("test_progress_protocol")) {
    return McpSystemControlHandlers::HandleTestProgressProtocol(
        this, RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("test_stale_progress")) {
    return McpSystemControlHandlers::HandleTestStaleProgress(
        this, RequestId, Payload, RequestingSocket);
  }
  if (Lower == TEXT("export_asset")) {
    return McpSystemControlHandlers::HandleExportAsset(this, RequestId, Payload,
                                                       RequestingSocket);
  }
  if (Lower == TEXT("execute_python")) {
    return McpSystemControlHandlers::HandleExecutePython(
        this, RequestId, Payload, RequestingSocket);
  }

  return false;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("System control actions require editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

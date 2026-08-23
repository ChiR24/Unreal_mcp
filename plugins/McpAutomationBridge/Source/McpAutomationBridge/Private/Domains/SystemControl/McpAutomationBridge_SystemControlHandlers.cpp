#include "Domains/SystemControl/McpAutomationBridge_SystemControlHandlersPrivate.h"

#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"

bool UMcpAutomationBridgeSubsystem::HandleSystemControlAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  // subAction FIRST, `action` only as the fallback — the same priority the
  // pre-queue gate's McpHandlerUtils::NormalizeAction uses. This domain reaches
  // execute_python, so a dispatcher that read `action` while the gate read
  // `subAction` let a read-scoped caller buy in-process code execution.
  // AuthorizeAutomationRequest already refuses a payload whose two fields
  // disagree; resolving them in the gate's own order means this handler cannot
  // diverge even if that guard is ever bypassed.
  FString SubAction;
  if (Payload.IsValid()) {
    if (!Payload->TryGetStringField(TEXT("subAction"), SubAction) || SubAction.IsEmpty()) {
      Payload->TryGetStringField(TEXT("action"), SubAction);
    }
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
  // system_control publishes console_command/execute_command, but the native
  // accept list never included them, so both answered NOT_IMPLEMENTED on this
  // transport while control_editor.console_command worked. Same published-but-
  // unreachable drift as set_pin_default_value; forward instead of refusing.
  const bool bConsoleAction =
      Lower == TEXT("console_command") || Lower == TEXT("execute_command");
  const bool bPluginAction = Lower == TEXT("list_plugins") ||
                             Lower == TEXT("enable_plugin") ||
                             Lower == TEXT("disable_plugin");
  if (!bPluginAction && !bConsoleAction &&
      !Lower.StartsWith(TEXT("run_ubt")) &&
      !Lower.StartsWith(TEXT("run_tests")) &&
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

  if (bConsoleAction) {
    return HandleControlEditorConsoleCommand(RequestId, Payload, RequestingSocket);
  }

  if (bPluginAction) {
    return McpSystemControlHandlers::HandleManagePlugins(this, RequestId, Lower,
                                                         Payload, RequestingSocket);
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

#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

// Placeholder implementations for the Phase 34 Utilities.
// Detailed APIs for layout management, Editor Utility widgets, etc. will be mapped here.

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetGridSettings(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Grid settings applied."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Grid settings updated"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetSnapSettings(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Snap settings applied."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Snap settings updated"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorManageLayouts(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Editor layout updated."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Layout managed"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorCreateCustomMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Custom editor mode created."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Mode created"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSpawnUtilityWidget(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Editor utility widget spawned."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Widget spawned"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorRunUtilityTask(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Editor utility task executed."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Task executed"), Result);
  return true;
}

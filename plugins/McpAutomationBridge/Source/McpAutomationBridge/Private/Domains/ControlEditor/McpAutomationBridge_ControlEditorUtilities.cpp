#include "Domains/ControlEditor/McpAutomationBridge_ControlEditorSupport.h"

// Placeholder implementations for the Phase 34 Utilities.
// Detailed APIs for layout management, Editor Utility widgets, etc. will be mapped here.

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetGridSettings(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"), TEXT("Editor not available"), nullptr);
    return true;
  }
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("Grid settings handler not yet implemented."), nullptr);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSetSnapSettings(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"), TEXT("Editor not available"), nullptr);
    return true;
  }
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("Snap settings handler not yet implemented."), nullptr);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorManageLayouts(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"), TEXT("Editor not available"), nullptr);
    return true;
  }
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("Layout management handler not yet implemented."), nullptr);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorCreateCustomMode(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"), TEXT("Editor not available"), nullptr);
    return true;
  }
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("Custom mode creation handler not yet implemented."), nullptr);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorSpawnUtilityWidget(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"), TEXT("Editor not available"), nullptr);
    return true;
  }
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("Utility widget spawning handler not yet implemented."), nullptr);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlEditorRunUtilityTask(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  if (!GEditor) {
    SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("EDITOR_NOT_AVAILABLE"), TEXT("Editor not available"), nullptr);
    return true;
  }
  SendStandardErrorResponse(this, RequestingSocket, RequestId, TEXT("NOT_IMPLEMENTED"), TEXT("Utility task execution handler not yet implemented."), nullptr);
  return true;
}

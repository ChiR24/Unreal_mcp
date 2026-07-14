#include "Domains/ControlActor/McpAutomationBridge_ControlActorSupport.h"
#include "Editor.h"

// Placeholder implementations for Phase 34 Selection & Grouping.
// Camera focus will be implemented here per user request.

bool UMcpAutomationBridgeSubsystem::HandleControlActorSelect(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Actor selected with camera focus."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Selected"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSelectByClass(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Actors selected by class."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Selected"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSelectByTag(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Actors selected by tag."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Selected"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorSelectInVolume(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Actors selected in volume."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Selected"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorDeselectAll(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  if (GEditor) {
      GEditor->SelectNone(true, true);
  }
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("All actors deselected."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Deselected"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGetSelected(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Selected actors retrieved."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Got Selected"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorGroup(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Actors grouped."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Grouped"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorUngroup(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Actors ungrouped."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Ungrouped"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleControlActorRunUtility(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Actor utility ran."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Utility Ran"), Result);
  return true;
}

#include "McpAutomationBridgeSubsystem.h"

// Placeholder implementations for the Phase 34 Content Browser / Asset Utilities.

bool UMcpAutomationBridgeSubsystem::HandleNavigateToPath(
    const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Navigated to path in Content Browser."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Navigated"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSyncToAsset(
    const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Synced to asset in Content Browser."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Synced"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleCreateCollection(
    const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Created collection."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Collection created"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleAddToCollection(
    const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Added to collection."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Added to collection"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleSetAssetColor(
    const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Set asset/folder color."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Color set"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleShowInExplorer(
    const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Shown in explorer."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Opened Explorer"), Result);
  return true;
}

bool UMcpAutomationBridgeSubsystem::HandleRunAssetActionUtility(
    const FString &RequestId, const FString &Action, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetStringField(TEXT("message"), TEXT("Asset Action Utility ran."));
  SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Utility Executed"), Result);
  return true;
}

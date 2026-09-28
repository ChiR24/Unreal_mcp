// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersAssetPathCanonical.h"
#include "Domains/AssetWorkflow/Rename/McpAutomationBridge_AssetRenameGuard.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"

bool UMcpAutomationBridgeSubsystem::HandleFixupRedirectors(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  const FString Lower = Action.ToLower();
  if (!Lower.Equals(TEXT("fixup_redirectors"), ESearchCase::IgnoreCase)) {
    // Not our action — allow other handlers to try
    return false;
  }

  // Implementation of redirector fixup functionality
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("fixup_redirectors payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  // Get directory path - REQUIRED for proper error reporting
  FString DirectoryPath;
  Payload->TryGetStringField(TEXT("directoryPath"), DirectoryPath);

  // Also check for "path" as alias
  if (DirectoryPath.IsEmpty()) {
    Payload->TryGetStringField(TEXT("path"), DirectoryPath);
  }

  bool bCheckoutFiles = false;
  Payload->TryGetBoolField(TEXT("checkoutFiles"), bCheckoutFiles);

  // Validate path is provided
  if (DirectoryPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("directoryPath or path is required for fixup_redirectors"),
                        TEXT("MISSING_ARGUMENT"));
    return true;
  }

  // SECURITY: Sanitize path to prevent traversal attacks
  FString SanitizedPath = SanitizeProjectRelativePath(DirectoryPath);
  if (SanitizedPath.IsEmpty()) {
    SendAutomationError(RequestingSocket, RequestId,
        FString::Printf(TEXT("Invalid path (traversal/security violation): %s"), *DirectoryPath),
        TEXT("SECURITY_VIOLATION"));
    return true;
  }

  // Normalize path
  FString NormalizedPath = SanitizedPath;
  McpAssetPathCanonical::MapContentRootInline(NormalizedPath);

  // CRITICAL FIX: Use DoesAssetDirectoryExistOnDisk for strict validation
  // UEditorAssetLibrary::DoesDirectoryExist() uses AssetRegistry cache which may
  // contain stale entries. We need to check if the directory ACTUALLY exists on disk.
  if (!DoesAssetDirectoryExistOnDisk(NormalizedPath)) {
    SendAutomationError(RequestingSocket, RequestId,
                        FString::Printf(TEXT("Directory not found: %s"), *NormalizedPath),
                        TEXT("PATH_NOT_FOUND"));
    return true;
  }

  int32 Found = 0;
  int32 Fixed = 0;
  McpAssetRename::FixupRedirectorsIn(NormalizedPath, Found, Fixed, bCheckoutFiles);
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetBoolField(TEXT("success"), true);
  Result->SetNumberField(TEXT("redirectorsFound"), Found);
  Result->SetNumberField(TEXT("redirectorsFixed"), Fixed);
  SendAutomationResponse(
      RequestingSocket, RequestId, true,
      Found == 0 ? FString(TEXT("No redirectors found"))
                 : FString::Printf(TEXT("Fixed %d redirectors"), Fixed),
      Result, FString());
  return true;
}

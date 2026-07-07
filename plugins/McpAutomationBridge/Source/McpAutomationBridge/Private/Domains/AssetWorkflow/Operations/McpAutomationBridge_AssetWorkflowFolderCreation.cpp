// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetDirectories.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleCreateFolder(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  FString Path;
  if (!Payload->TryGetStringField(TEXT("path"), Path) || Path.IsEmpty()) {
    Payload->TryGetStringField(TEXT("directoryPath"), Path);
  }

  if (Path.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("path (or directoryPath) required"), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString SafePath = SanitizeProjectRelativePath(Path);
  if (SafePath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Invalid path: must be project-relative and not contain '..'"),
        nullptr, TEXT("INVALID_PATH"));
    return true;
  }

  if (UEditorAssetLibrary::DoesDirectoryExist(SafePath) ||
      UEditorAssetLibrary::MakeDirectory(SafePath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("path"), SafePath);
    // A folder is a directory, not an asset object — VerifyAssetExists looks for a .uasset
    // and so always reported existsAfter:false for a freshly created folder. Report the
    // actual directory existence via the on-disk check (asset-registry directory entries can
    // be stale; this mirrors the codebase's DoesAssetDirectoryExistOnDisk convention). Empty
    // folders may still not persist across an editor restart until an asset is added, but the
    // in-session readback is now truthful.
    Resp->SetBoolField(TEXT("existsAfter"), DoesAssetDirectoryExistOnDisk(SafePath));
    SendAutomationResponse(Socket, RequestId, true, TEXT("Folder created"),
                           Resp, FString());
  } else {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to create folder"), nullptr,
                           TEXT("CREATE_FAILED"));
  }
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to get asset dependencies.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath' and optional 'recursive'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */

// Copyright (c) 2024 EngineWire Contributors

#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#include "EditorAssetLibrary.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetDirs.h"

bool UEngineWireSubsystem::HandleCreateFolder(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString Path;
  if (!McpReadRequiredField(this, Socket, RequestId, Payload, TEXT("path"), Path)) {
    return true;
  }

  FString SafePath = SanitizeProjectRelativePath(Path);
  if (SafePath.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        McpPathRefusalMessage(TEXT("path"), Path),
        nullptr, TEXT("INVALID_PATH"));
    return true;
  }

  // Check FIRST: the success branch below creates the folder, so testing
  // existence there would always read true. A repeat call is idempotent, not
  // fresh work, and the response must say which one happened.
  const bool bAlreadyExisted = UEditorAssetLibrary::DoesDirectoryExist(SafePath);
  if (bAlreadyExisted ||
      UEditorAssetLibrary::MakeDirectory(SafePath)) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetStringField(TEXT("path"), SafePath);
    // A folder is a directory, not an asset object — VerifyAssetExists looks for a .uasset
    // and so always reported existsAfter:false for a freshly created folder. Report the
    // actual directory existence via the on-disk check (asset-registry directory entries can
    // be stale; this mirrors the codebase's DoesAssetDirectoryExistOnDisk convention). Empty
    // folders may still not persist across an editor restart until an asset is added, but the
    // in-session readback is now truthful.
    Resp->SetBoolField(TEXT("existsAfter"), DoesAssetDirectoryExistOnDisk(SafePath));
    Resp->SetBoolField(TEXT("alreadyExisted"), bAlreadyExisted);
    SendAutomationResponse(Socket, RequestId, true,
                           bAlreadyExisted ? TEXT("Folder already exists") : TEXT("Folder created"),
                           Resp, FString());
  } else {
    SendAutomationError(Socket, RequestId,
                        TEXT("Failed to create folder"),
                        TEXT("CREATE_FAILED"));
  }
  return true;
}

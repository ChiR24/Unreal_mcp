// Copyright (c) 2024 EngineWire Contributors

#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#include "EditorAssetLibrary.h"

bool UEngineWireSubsystem::HandleSetTags(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString SafeAssetPath;
  if (!McpReadAssetPathOrReply(this, Socket, RequestId, Payload, SafeAssetPath, /*bMustExist=*/false))
    return true;

  const TArray<FString> Tags = McpHandlerUtils::GetStringArrayField(Payload, TEXT("tags"));

  // Edge-case: empty or missing tags array should be treated as a no-op
  // success.
  if (Tags.Num() == 0) {
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
    Resp->SetNumberField(TEXT("appliedTags"), 0);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("No tags provided; no-op"), Resp, FString());
    return true;
  }

  if (!McpAssetExists(SafeAssetPath)) {
    SendAutomationError(Socket, RequestId, TEXT("Asset not found"),
                        TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = McpLoadAsset(SafeAssetPath);
  if (!Asset) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Failed to load asset"),
                        TEXT("LOAD_FAILED"));
    return true;
  }

  // Implement set_tags by mapping them to Package Metadata (Tag=true)
  int32 AppliedCount = 0;
  for (const FString &Tag : Tags) {
    UEditorAssetLibrary::SetMetadataTag(Asset, FName(*Tag), TEXT("true"));
    AppliedCount++;
  }

  // Mark dirty so the asset can be saved later
  Asset->MarkPackageDirty();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetBoolField(TEXT("markedDirty"), true);
  Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
  Resp->SetNumberField(TEXT("appliedTags"), AppliedCount);
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Tags applied as metadata"), Resp, FString());

  return true;
}

/**
 * Handles requests to validate if an asset exists and can be loaded.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UEngineWireSubsystem::HandleValidateAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString SafeAssetPath;
  if (!McpReadAssetPathOrReply(this, Socket, RequestId, Payload, SafeAssetPath))
    return true;

  UObject *Asset = McpLoadAsset(SafeAssetPath);
  if (!Asset) {
    SendAutomationError(Socket, RequestId,
                        TEXT("Failed to load asset"),
                        TEXT("LOAD_FAILED"));
    return true;
  }

  bool bIsValid = true;
  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), bIsValid);
  Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
  Resp->SetBoolField(TEXT("isValid"), bIsValid);

  SendAutomationResponse(Socket, RequestId, true, TEXT("Asset validated"),
                         Resp, FString());
  return true;
}


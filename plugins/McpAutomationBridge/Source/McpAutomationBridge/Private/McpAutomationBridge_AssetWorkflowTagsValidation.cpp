// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleSetTags(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("set_tags payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const TArray<TSharedPtr<FJsonValue>> *TagsArray = nullptr;
  TArray<FString> Tags;
  if (Payload->TryGetArrayField(TEXT("tags"), TagsArray) && TagsArray) {
    for (const TSharedPtr<FJsonValue> &Val : *TagsArray) {
      if (Val.IsValid() && Val->Type == EJson::String) {
        Tags.Add(Val->AsString());
      }
    }
  }

  const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, Socket, SafeAssetPath,
                                         Tags]() {
    UMcpAutomationBridgeSubsystem *StrongThis = WeakThis.Get();
    if (!StrongThis) {
      return;
    }
    // Edge-case: empty or missing tags array should be treated as a no-op
    // success.
    if (Tags.Num() == 0) {
      TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
      Resp->SetBoolField(TEXT("success"), true);
      Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
      Resp->SetNumberField(TEXT("appliedTags"), 0);
      StrongThis->SendAutomationResponse(Socket, RequestId, true,
                             TEXT("No tags provided; no-op"), Resp, FString());
      return;
    }

    if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
      StrongThis->SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                             nullptr, TEXT("ASSET_NOT_FOUND"));
      return;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
    if (!Asset) {
      StrongThis->SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Failed to load asset"), nullptr,
                             TEXT("LOAD_FAILED"));
      return;
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
    StrongThis->SendAutomationResponse(Socket, RequestId, true,
                           TEXT("Tags applied as metadata"), Resp, FString());
  });

  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to validate if an asset exists and can be loaded.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */
bool UMcpAutomationBridgeSubsystem::HandleValidateAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("validate payload missing"), nullptr,
                           TEXT("INVALID_PAYLOAD"));
    return true;
  }

  FString AssetPath;
  Payload->TryGetStringField(TEXT("assetPath"), AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("assetPath required"),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const FString SafeAssetPath = SanitizeProjectRelativePath(AssetPath);
  if (SafeAssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, Socket, SafeAssetPath]() {
    UMcpAutomationBridgeSubsystem *StrongThis = WeakThis.Get();
    if (!StrongThis) {
      return;
    }
    if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
      StrongThis->SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                             nullptr, TEXT("ASSET_NOT_FOUND"));
      return;
    }

    UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
    if (!Asset) {
      StrongThis->SendAutomationResponse(Socket, RequestId, false,
                             TEXT("Failed to load asset"), nullptr,
                             TEXT("LOAD_FAILED"));
      return;
    }

    bool bIsValid = true;
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), bIsValid);
    Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
    Resp->SetBoolField(TEXT("isValid"), bIsValid);

    StrongThis->SendAutomationResponse(Socket, RequestId, true, TEXT("Asset validated"),
                           Resp, FString());
  });
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to list assets with filtering and pagination.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing filter criteria and pagination
 * options.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */

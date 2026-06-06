// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleSetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("set_metadata payload missing"), nullptr,
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

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  const FString SafeAssetPath = AssetPath;

  if (!UEditorAssetLibrary::DoesAssetExist(SafeAssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  const TSharedPtr<FJsonObject> *MetadataObjPtr = nullptr;
  if (!Payload->TryGetObjectField(TEXT("metadata"), MetadataObjPtr) ||
      !MetadataObjPtr) {
    // Treat missing/empty metadata as a no-op success; nothing to write.
    TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
    Resp->SetNumberField(TEXT("updatedKeys"), 0);
    SendAutomationResponse(Socket, RequestId, true,
                           TEXT("No metadata provided; no-op"), Resp,
                           FString());
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(SafeAssetPath);
  if (!Asset) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  UPackage *Package = Asset->GetOutermost();
  if (!Package) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to resolve package for asset"), nullptr,
                           TEXT("PACKAGE_NOT_FOUND"));
    return true;
  }

  // GetMetaData returns the metadata object that is owned by this package.
  // UE 5.0 uses UMetaData*, UE 5.6+ uses FMetaData&
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
  FMetaData& Meta = Package->GetMetaData();
#else
  UMetaData* Meta = Package->GetMetaData();
#endif

  const TSharedPtr<FJsonObject> &MetadataObj = *MetadataObjPtr;
  int32 UpdatedCount = 0;

  for (const auto &Kvp : MetadataObj->Values) {
    const FString Key(*Kvp.Key);
    const TSharedPtr<FJsonValue> &Val = Kvp.Value;

    FString ValueString;
    if (!Val.IsValid() || Val->IsNull()) {
      continue;
    }
    switch (Val->Type) {
    case EJson::String:
      ValueString = Val->AsString();
      break;
    case EJson::Number:
      ValueString = LexToString(Val->AsNumber());
      break;
    case EJson::Boolean:
      ValueString = Val->AsBool() ? TEXT("true") : TEXT("false");
      break;
    default:
      // For arrays/objects, store a compact JSON string
      {
        FString JsonOut;
        const TSharedRef<TJsonWriter<>> Writer =
            TJsonWriterFactory<>::Create(&JsonOut);
        FJsonSerializer::Serialize(Val, TEXT(""), Writer);
        ValueString = JsonOut;
      }
      break;
    }

    if (!ValueString.IsEmpty()) {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
      Meta.SetValue(Asset, *Key, *ValueString);
#else
      Meta->SetValue(Asset, *Key, *ValueString);
#endif
      ++UpdatedCount;
    }
  }

  if (UpdatedCount > 0) {
    Package->SetDirtyFlag(true);
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), SafeAssetPath);
  Resp->SetNumberField(TEXT("updatedKeys"), UpdatedCount);

  // Add verification data
  McpHandlerUtils::AddVerification(Resp, Asset);

  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Asset metadata updated"), Resp, FString());
  return true;
#else
  SendAutomationError(RequestingSocket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles asset duplication requests. Supports both single asset and folder
 * (deep) duplication.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'sourcePath' and 'destinationPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */

bool UMcpAutomationBridgeSubsystem::HandleGetMetadata(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("get_metadata payload missing"), nullptr,
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

  AssetPath = SanitizeProjectRelativePath(AssetPath);
  if (AssetPath.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Invalid assetPath"), nullptr,
                           TEXT("SECURITY_VIOLATION"));
    return true;
  }

  if (!UEditorAssetLibrary::DoesAssetExist(AssetPath)) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("Asset not found"),
                           nullptr, TEXT("ASSET_NOT_FOUND"));
    return true;
  }

  UObject *Asset = UEditorAssetLibrary::LoadAsset(AssetPath);
  if (!Asset) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Failed to load asset"), nullptr,
                           TEXT("LOAD_FAILED"));
    return true;
  }

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetStringField(TEXT("assetPath"), AssetPath);

  // 1. Asset Registry Tags
  FAssetData AssetData(Asset);
  TSharedPtr<FJsonObject> TagsObj = McpHandlerUtils::CreateResultObject();
  for (const auto &Kvp : AssetData.TagsAndValues) {
    TagsObj->SetStringField(Kvp.Key.ToString(), Kvp.Value.AsString());
  }
  Resp->SetObjectField(TEXT("tags"), TagsObj);

  // 2. Package Metadata information
  UPackage *Package = Asset->GetOutermost();
  if (Package) {

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6
    FMetaData& Meta = Package->GetMetaData();
    bool bHasMeta = FMetaData::GetMapForObject(Asset) != nullptr;
    Resp->SetBoolField(TEXT("debug_has_meta"), bHasMeta);

    const TMap<FName, FString> *ObjectMeta = FMetaData::GetMapForObject(Asset);
#else
    UMetaData* Meta = Package->GetMetaData();
    bool bHasMeta = Meta->GetMapForObject(Asset) != nullptr;
    Resp->SetBoolField(TEXT("debug_has_meta"), bHasMeta);

    const TMap<FName, FString> *ObjectMeta = Meta->GetMapForObject(Asset);
#endif
    if (ObjectMeta) {
      TSharedPtr<FJsonObject> MetaObj = McpHandlerUtils::CreateResultObject();
      for (const auto &Entry : *ObjectMeta) {
        MetaObj->SetStringField(Entry.Key.ToString(), Entry.Value);
      }
      Resp->SetObjectField(TEXT("metadata"), MetaObj);
    }
  }

  SendAutomationResponse(Socket, RequestId, true, TEXT("Metadata retrieved"),
                         Resp, FString());
  return true;
#else
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("get_metadata requires editor build"), nullptr,
                         TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

// ============================================================================
// 9. NANITE/MESH WORKFLOW ACTIONS
// ============================================================================

// Dispatcher-compatible mesh workflow handlers with explicit success/error responses.

// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowContentSourceRoots.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpFabProvider.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

#if WITH_EDITOR

namespace
{
/** Absolute path of the Bridge library index. */
FString MegascansIndexPath()
{
	return FPaths::Combine(McpContentSources::MegascansLibraryDir(), TEXT("uassetsData.json"));
}
} // namespace

/**
 * Lists the Bridge/Megascans library index.
 *
 * Bridge maintains `uassetsData.json` next to the downloaded packs, so unlike
 * Fab — whose catalog only exists inside an authenticated web view — the
 * Megascans inventory is a plain local read.
 */
bool UMcpAutomationBridgeSubsystem::HandleListMegascansLibrary(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  const FString LibraryDir = McpContentSources::MegascansLibraryDir();
  const FString IndexPath = MegascansIndexPath();

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("libraryDirectory"), LibraryDir);
  Result->SetStringField(TEXT("indexPath"), IndexPath);

  FString Raw;
  const bool bIndexExists = FFileHelper::LoadFileToString(Raw, *IndexPath);
  Result->SetBoolField(TEXT("indexExists"), bIndexExists);

  TArray<TSharedPtr<FJsonValue>> Assets;
  if (bIndexExists) {
    TSharedPtr<FJsonObject> Index;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (FJsonSerializer::Deserialize(Reader, Index) && Index.IsValid()) {
      const TArray<TSharedPtr<FJsonValue>> *Found = nullptr;
      if (Index->TryGetArrayField(TEXT("assets"), Found) && Found != nullptr) {
        Assets = *Found;
      }
    }
  }

  FString Filter;
  Payload->TryGetStringField(TEXT("filter"), Filter);
  if (!Filter.IsEmpty()) {
    Assets.RemoveAll([&Filter](const TSharedPtr<FJsonValue> &Value) {
      FString Serialized;
      const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
      FJsonSerializer::Serialize(Value, FString(), Writer);
      return !Serialized.Contains(Filter);
    });
  }

  Result->SetArrayField(TEXT("assets"), Assets);
  Result->SetNumberField(TEXT("assetCount"), Assets.Num());
  IMcpFabProvider *Provider = GetMcpFabProvider();
  Result->SetBoolField(TEXT("importAvailable"),
                       Provider != nullptr && Provider->IsMegascansAvailable());
  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Megascans library holds %d indexed asset(s) in %s."),
                      Assets.Num(), *LibraryDir),
      Result);
  return true;
}

/**
 * Imports a downloaded Megascans pack through the Bridge plugin's own importer.
 *
 * FAssetsImportController::DataReceived is exported (MEGASCANSPLUGIN_API) and
 * takes the same JSON the Bridge desktop app sends over its Node socket, so the
 * import runs headlessly — no Bridge window, no drag, no sign-in. It imports
 * content already on disk; downloading remains the Bridge app's job.
 */
bool UMcpAutomationBridgeSubsystem::HandleImportMegascansAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  IMcpFabProvider *Provider = GetMcpFabProvider();
  if (Provider == nullptr || !Provider->IsMegascansAvailable()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Megascans support is not loaded in this editor, so Bridge imports are unavailable."),
        nullptr, TEXT("PLUGIN_UNAVAILABLE"));
    return true;
  }

  TSharedPtr<FJsonObject> Envelope;
  const TSharedPtr<FJsonObject> *Provided = nullptr;
  if (Payload->TryGetObjectField(TEXT("payload"), Provided) && Provided != nullptr) {
    Envelope = MakeShared<FJsonObject>(**Provided);
  } else {
    // Synthesize the single-asset envelope Bridge would have sent.
    const TArray<TSharedPtr<FJsonValue>> *AssetPaths = nullptr;
    if (!Payload->TryGetArrayField(TEXT("assetPaths"), AssetPaths) || AssetPaths == nullptr ||
        AssetPaths->Num() == 0) {
      SendAutomationResponse(
          Socket, RequestId, false,
          TEXT("Provide either 'payload' (a full Bridge export envelope) or 'assetPaths' plus 'folderName'."),
          nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }
    FString FolderName;
    Payload->TryGetStringField(TEXT("folderName"), FolderName);
    if (FolderName.IsEmpty()) {
      SendAutomationResponse(Socket, RequestId, false,
                             TEXT("'folderName' is required when synthesizing a payload."),
                             nullptr, TEXT("INVALID_ARGUMENT"));
      return true;
    }
    FString AssetType = TEXT("3d");
    Payload->TryGetStringField(TEXT("assetType"), AssetType);
    FString ExportMode = TEXT("normal");
    Payload->TryGetStringField(TEXT("exportMode"), ExportMode);
    FString AssetId = FolderName;
    Payload->TryGetStringField(TEXT("assetId"), AssetId);
    FString AssetName = FolderName;
    Payload->TryGetStringField(TEXT("name"), AssetName);

    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("assetId"), AssetId);
    Entry->SetStringField(TEXT("assetType"), AssetType);
    Entry->SetStringField(TEXT("exportMode"), ExportMode);
    Entry->SetStringField(TEXT("exportType"), TEXT("uasset"));
    Entry->SetStringField(TEXT("folderName"), FolderName);
    Entry->SetStringField(TEXT("name"), AssetName);
    Entry->SetArrayField(TEXT("assetPaths"), *AssetPaths);

    Envelope = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Entries;
    Entries.Add(MakeShared<FJsonValueObject>(Entry));
    Envelope->SetArrayField(TEXT("exportPayload"), Entries);
  }

  if (!Envelope->HasTypedField<EJson::Array>(TEXT("exportPayload"))) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("'payload' must contain an 'exportPayload' array."),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString Serialized;
  const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
  FJsonSerializer::Serialize(Envelope.ToSharedRef(), Writer);

  // The importer spawns editor work; keep it on the game thread like every
  // other asset mutation this bridge performs.
  FString ImportError;
  if (!Provider->ImportMegascansEnvelope(Serialized, ImportError)) {
    SendAutomationResponse(Socket, RequestId, false, ImportError, nullptr,
                           TEXT("PLUGIN_UNAVAILABLE"));
    return true;
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetNumberField(TEXT("entryCount"),
                         Envelope->GetArrayField(TEXT("exportPayload")).Num());
  Result->SetStringField(TEXT("note"), TEXT("Import dispatched to the Bridge importer; assets appear under /Game/Megascans."));
  SendAutomationResponse(Socket, RequestId, true,
                         TEXT("Megascans import dispatched."), Result);
  return true;
}
#else
bool UMcpAutomationBridgeSubsystem::HandleListMegascansLibrary(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  SendAutomationResponse(Socket, RequestId, false, TEXT("Editor required."), nullptr,
                         TEXT("EDITOR_ONLY"));
  return true;
}
bool UMcpAutomationBridgeSubsystem::HandleImportMegascansAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  SendAutomationResponse(Socket, RequestId, false, TEXT("Editor required."), nullptr,
                         TEXT("EDITOR_ONLY"));
  return true;
}
#endif

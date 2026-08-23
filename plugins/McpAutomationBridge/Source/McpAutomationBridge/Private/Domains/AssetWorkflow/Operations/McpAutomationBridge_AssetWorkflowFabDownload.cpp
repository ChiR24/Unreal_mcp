// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowContentSourceRoots.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpFabProvider.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#if WITH_EDITOR

/**
 * Downloads a Fab asset through the Fab plugin's own downloader.
 *
 * The transfer runs on Fab's HTTP/BuildPatchServices path — the same one the Fab
 * tab uses — rather than a parallel implementation that would miss its retry and
 * BuildPatch handling. That call lives in the adapter module so this one never
 * imports Fab.dll; see McpFabProvider.h.
 *
 * The signed `downloadUrl` is NOT minted here. Fab issues it from the
 * authenticated web session, and no C++ entry point initiates that; supply the
 * URL from the Fab tab's own flow or from an account-scoped API call. Once the
 * pack lands, asset.migrate_assets places it into /Game.
 */
bool UMcpAutomationBridgeSubsystem::HandleDownloadFabAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  IMcpFabProvider *Provider = GetMcpFabProvider();
  if (Provider == nullptr || !Provider->IsFabAvailable()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("Fab support is not loaded in this editor, so Fab downloads are unavailable."),
        nullptr, TEXT("NOT_SUPPORTED"));
    return true;
  }

  FString AssetId, DownloadUrl;
  if (!Payload->TryGetStringField(TEXT("assetId"), AssetId) || AssetId.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false, TEXT("'assetId' is required."),
                           nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!Payload->TryGetStringField(TEXT("downloadUrl"), DownloadUrl) ||
      !DownloadUrl.StartsWith(TEXT("https://"))) {
    SendAutomationResponse(
        Socket, RequestId, false,
        TEXT("'downloadUrl' is required and must be https. Fab mints this from its authenticated session; it cannot be produced here."),
        nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString Destination = McpContentSources::FabLibraryDir();
  Payload->TryGetStringField(TEXT("destinationDirectory"), Destination);
  if (Destination.IsEmpty()) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Could not resolve a destination directory."), nullptr,
                           TEXT("INVALID_ARGUMENT"));
    return true;
  }
  IFileManager::Get().MakeDirectory(*Destination, true);

  // BuildPatchServices is what Marketplace-era packs ship through; plain HTTP
  // covers the rest. Default to HTTP and let the caller opt in, because a
  // BuildPatch request against a plain URL stalls rather than failing.
  FString DownloadType = TEXT("http");
  Payload->TryGetStringField(TEXT("downloadType"), DownloadType);
  const bool bUseBuildPatch =
      DownloadType.Equals(TEXT("buildpatch"), ESearchCase::IgnoreCase);

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakThis(this);
  const bool bQueued = Provider->EnqueueDownload(
      AssetId, DownloadUrl, Destination, bUseBuildPatch,
      [WeakThis, RequestId, Socket, Destination](const FMcpFabDownloadResult &Stats) {
        // The queue completes off the game thread; every response this bridge
        // sends is built there, so hop before touching the subsystem.
        AsyncTask(ENamedThreads::GameThread, [WeakThis, RequestId, Socket, Destination, Stats]() {
          UMcpAutomationBridgeSubsystem *Self = WeakThis.Get();
          if (Self == nullptr) {
            return;
          }
          TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
          Result->SetBoolField(TEXT("downloadSucceeded"), Stats.bSuccess);
          Result->SetBoolField(TEXT("servedFromCache"), Stats.bServedFromCache);
          Result->SetNumberField(TEXT("completedBytes"), static_cast<double>(Stats.CompletedBytes));
          Result->SetNumberField(TEXT("totalBytes"), static_cast<double>(Stats.TotalBytes));
          Result->SetStringField(TEXT("destinationDirectory"), Destination);
          TArray<TSharedPtr<FJsonValue>> Files;
          for (const FString &File : Stats.DownloadedFiles) {
            Files.Add(MakeShared<FJsonValueString>(File));
          }
          Result->SetArrayField(TEXT("downloadedFiles"), Files);
          Result->SetStringField(
              TEXT("note"),
              TEXT("Use asset.migrate_assets with sourceRoot 'fabLibrary' to place the pack into /Game."));
          Self->SendAutomationResponse(
              Socket, RequestId, Stats.bSuccess,
              Stats.bSuccess
                  ? FString::Printf(TEXT("Downloaded %llu byte(s)."), Stats.CompletedBytes)
                  : TEXT("Fab download failed; the signed URL may have expired."),
              Result, Stats.bSuccess ? TEXT("") : TEXT("DOWNLOAD_FAILED"));
        });
      });

  if (!bQueued) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("Fab refused the download request."), nullptr,
                           TEXT("DOWNLOAD_FAILED"));
  }
  return true;
}
#else
bool UMcpAutomationBridgeSubsystem::HandleDownloadFabAsset(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  SendAutomationResponse(Socket, RequestId, false, TEXT("Editor required."), nullptr,
                         TEXT("EDITOR_ONLY"));
  return true;
}
#endif

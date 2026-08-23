// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowContentSourceRoots.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpFabProvider.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#if WITH_EDITOR

/**
 * Reports what the Fab plugin has already downloaded to this machine.
 *
 * This deliberately reads only local state. Fab's catalog is not on disk — the
 * plugin's browser is an authenticated web view that fetches listings and
 * short-lived signed download URLs, then calls back into C++ with them. Listing
 * what is *purchasable* would therefore mean re-implementing that web API
 * against a private, undocumented surface; listing what is *downloaded* is a
 * directory read, and it is the half that lets an agent find and migrate an
 * asset after the operator pulls it down through the Fab UI.
 *
 * Unlike the other Fab actions this one still answers without the adapter: the
 * cache directory is a plain path, so a scan of it beats claiming there is
 * nothing there.
 */
bool UMcpAutomationBridgeSubsystem::HandleListFabDownloads(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  TArray<TSharedPtr<FJsonValue>> Downloads;

  FString CacheDir = McpContentSources::FabLibraryDir();
  IMcpFabProvider *Provider = GetMcpFabProvider();
  const bool bPluginAvailable = Provider != nullptr && Provider->IsFabAvailable();

  if (bPluginAvailable) {
    // The plugin's own accessor wins over the config probe: it reflects a
    // location the user changed this session, before the ini is flushed.
    const FString PluginCacheDir = Provider->GetCacheLocation();
    if (!PluginCacheDir.IsEmpty()) {
      CacheDir = FPaths::ConvertRelativePathToFull(PluginCacheDir);
    }
    TArray<FMcpFabCachedAsset> Cached;
    Provider->GetCachedAssets(Cached);
    for (const FMcpFabCachedAsset &Asset : Cached) {
      TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
      Entry->SetStringField(TEXT("assetId"), Asset.AssetId);
      Entry->SetStringField(TEXT("cachedFile"), Asset.CachedFile);
      Downloads.Add(MakeShared<FJsonValueObject>(Entry));
    }
  } else {
    // Without the adapter the cache directory is still readable, so report the
    // archives found there rather than claiming there is nothing.
    TArray<FString> Archives;
    IFileManager::Get().FindFiles(Archives, *(CacheDir / TEXT("*.zip")), true, false);
    Archives.Sort();
    for (const FString &Archive : Archives) {
      TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
      Entry->SetStringField(TEXT("assetId"), FPaths::GetBaseFilename(Archive));
      Entry->SetStringField(TEXT("cachedFile"), CacheDir / Archive);
      Downloads.Add(MakeShared<FJsonValueObject>(Entry));
    }
  }

  const bool bCacheExists = IFileManager::Get().DirectoryExists(*CacheDir);
  Result->SetArrayField(TEXT("downloads"), Downloads);
  Result->SetNumberField(TEXT("downloadCount"), Downloads.Num());
  Result->SetStringField(TEXT("cacheDirectory"), CacheDir);
  Result->SetBoolField(TEXT("cacheDirectoryExists"), bCacheExists);
  Result->SetBoolField(TEXT("fabModuleAvailable"), bPluginAvailable);
  Result->SetStringField(
      TEXT("note"),
      Downloads.Num() > 0
          ? TEXT("Downloaded packs land under the fabLibrary source root; migrate them with asset.migrate_assets.")
          : TEXT("Nothing downloaded yet. Browsing and purchasing happen in the editor's Fab tab; this reports what that leaves on disk."));
  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Found %d Fab download(s) in %s."), Downloads.Num(), *CacheDir),
      Result);
  return true;
}
#else
bool UMcpAutomationBridgeSubsystem::HandleListFabDownloads(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("list_fab_downloads requires the editor."), nullptr,
                         TEXT("EDITOR_ONLY"));
  return true;
}
#endif

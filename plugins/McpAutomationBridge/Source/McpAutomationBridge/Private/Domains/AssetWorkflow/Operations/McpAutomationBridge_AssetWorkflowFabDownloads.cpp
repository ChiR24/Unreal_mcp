// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowContentSourceRoots.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "McpFabProvider.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"


/**
 * Reports what the Fab plugin has already downloaded to this machine.
 *
 * This deliberately reads only local state: what Fab has finished downloading
 * into its cache. Finding a listing is the catalog search and adding one is the
 * add; listing what is *downloaded* is a directory read, and it is the half that
 * lets an agent find and migrate a pack that has already been pulled down.
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
          : TEXT("Nothing downloaded yet. Add a listing with asset.import_marketplace_asset (marketplace=fab_listing) and follow it with lookup=fab_import_status; this reports the archives Fab has finished downloading into its cache."));
  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Found %d Fab download(s) in %s."), Downloads.Num(), *CacheDir),
      Result);
  return true;
}

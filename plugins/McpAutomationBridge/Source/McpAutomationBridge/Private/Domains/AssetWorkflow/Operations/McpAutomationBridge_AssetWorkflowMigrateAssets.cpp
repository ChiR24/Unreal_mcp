// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowContentSourceRoots.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"

namespace
{
/** Package files and their editor-side sidecars, relative to Dir. */
void FindPackageFiles(const FString& Dir, TArray<FString>& OutRelative)
{
	static const TCHAR* Patterns[] = {TEXT("*.uasset"), TEXT("*.umap"), TEXT("*.uexp"), TEXT("*.ubulk")};
	for (const TCHAR* Pattern : Patterns)
	{
		TArray<FString> Found;
		IFileManager::Get().FindFilesRecursive(Found, *Dir, Pattern, true, false, false);
		for (const FString& Abs : Found)
		{
			FString Relative = Abs;
			FPaths::MakePathRelativeTo(Relative, *(Dir / TEXT("")));
			OutRelative.Add(Relative);
		}
	}
	OutRelative.Sort();
}
} // namespace

bool UMcpAutomationBridgeSubsystem::HandleMigrateAssets(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString SourceRoot;
  if (!Payload->TryGetStringField(TEXT("sourceRoot"), SourceRoot) || SourceRoot.IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("sourceRoot required. Allowed: %s. Use list_content_sources to discover ids."),
                        *FString::Join(McpContentSources::AllRootTokens(), TEXT(", "))),
        nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  FString SourceId;
  Payload->TryGetStringField(TEXT("sourceId"), SourceId);

  FString SourceDir, ResolveError;
  if (!McpContentSources::ResolveSourceDir(SourceRoot, SourceId, SourceDir, ResolveError)) {
    SendAutomationResponse(Socket, RequestId, false, ResolveError, nullptr,
                           ResolveError.StartsWith(TEXT("SECURITY_VIOLATION"))
                               ? TEXT("SECURITY_VIOLATION")
                               : TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (!IFileManager::Get().DirectoryExists(*SourceDir)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source directory does not exist: %s"), *SourceDir),
        nullptr, TEXT("NOT_FOUND"));
    return true;
  }

  FString ContentDir = McpContentSources::ContentDirFor(SourceDir);
  FString SubPath;
  if (Payload->TryGetStringField(TEXT("subPath"), SubPath) && !SubPath.IsEmpty()) {
    FString ScopedDir, SubError;
    if (!McpContentSources::ResolveSourceDir(SourceRoot,
                                             SourceId.IsEmpty() ? SubPath : SourceId / SubPath,
                                             ScopedDir, SubError)) {
      SendAutomationResponse(Socket, RequestId, false, SubError, nullptr, TEXT("SECURITY_VIOLATION"));
      return true;
    }
    const FString Scoped = FPaths::Combine(ContentDir, SubPath);
    ContentDir = IFileManager::Get().DirectoryExists(*Scoped) ? Scoped : ScopedDir;
  }

  TArray<FString> RelativeFiles;
  FindPackageFiles(ContentDir, RelativeFiles);
  if (RelativeFiles.Num() == 0) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("No .uasset/.umap packages found under %s"), *ContentDir),
        nullptr, TEXT("NOT_FOUND"));
    return true;
  }

  // /Game subfolder the source tree lands in. Default "/Game" reproduces the
  // source's own layout, which is the only arrangement that keeps the
  // /Game/... references stored INSIDE the copied packages resolvable — a raw
  // package copy cannot rewrite them.
  FString DestinationPath = TEXT("/Game");
  Payload->TryGetStringField(TEXT("destinationPath"), DestinationPath);
  if (!DestinationPath.StartsWith(TEXT("/Game"))) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("destinationPath must be under /Game."), nullptr,
                           TEXT("INVALID_PATH"));
    return true;
  }
  if (DestinationPath.Contains(TEXT(".."))) {
    SendAutomationResponse(Socket, RequestId, false,
                           TEXT("SECURITY_VIOLATION: destinationPath must not contain '..'"),
                           nullptr, TEXT("SECURITY_VIOLATION"));
    return true;
  }
  FString DestRelative = DestinationPath;
  DestRelative.RemoveFromStart(TEXT("/Game"));
  DestRelative.RemoveFromStart(TEXT("/"));
  const bool bStructurePreserved = DestRelative.IsEmpty();

  double MaxPackages = 4000;
  Payload->TryGetNumberField(TEXT("maxPackages"), MaxPackages);
  if (RelativeFiles.Num() > static_cast<int32>(MaxPackages)) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Source holds %d files, above maxPackages %d. Narrow with subPath or raise maxPackages."),
                        RelativeFiles.Num(), static_cast<int32>(MaxPackages)),
        nullptr, TEXT("LIMIT_EXCEEDED"));
    return true;
  }

  bool bOverwrite = false;
  Payload->TryGetBoolField(TEXT("overwrite"), bOverwrite);
  bool bDryRun = false;
  Payload->TryGetBoolField(TEXT("dryRun"), bDryRun);

  const FString DestContentDir =
      FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectContentDir(), DestRelative));

  int32 Copied = 0, Skipped = 0, Failed = 0;
  TArray<TSharedPtr<FJsonValue>> Sample;
  TArray<TSharedPtr<FJsonValue>> Failures;
  for (const FString &Relative : RelativeFiles) {
    const FString Destination = FPaths::Combine(DestContentDir, Relative);
    const bool bExists = IFileManager::Get().FileExists(*Destination);
    if (bExists && !bOverwrite) {
      ++Skipped;
      continue;
    }
    if (Sample.Num() < 40) {
      FString Package = FPaths::Combine(DestinationPath, Relative);
      Package = FPaths::SetExtension(Package, TEXT(""));
      Package.ReplaceInline(TEXT("\\"), TEXT("/"));
      Sample.Add(MakeShared<FJsonValueString>(Package));
    }
    if (bDryRun) {
      ++Copied;
      continue;
    }
    const uint32 Result = IFileManager::Get().Copy(
        *Destination, *FPaths::Combine(ContentDir, Relative), bOverwrite, true, false);
    if (Result == COPY_OK) {
      ++Copied;
    } else {
      ++Failed;
      if (Failures.Num() < 20) {
        Failures.Add(MakeShared<FJsonValueString>(Relative));
      }
    }
  }

  if (!bDryRun && Copied > 0) {
    FAssetRegistryModule &Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    Registry.Get().ScanPathsSynchronous({DestinationPath}, /*bForceRescan*/ true);
  }

  TArray<TSharedPtr<FJsonValue>> Warnings;
  if (!bStructurePreserved) {
    Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(
        TEXT("destinationPath '%s' relocates the tree. References stored inside the copied packages still point at their original /Game paths and will resolve only if those paths also exist. Use the default '/Game' to keep them intact."),
        *DestinationPath)));
  }
  if (Failed > 0) {
    Warnings.Add(MakeShared<FJsonValueString>(
        TEXT("Some files failed to copy; they may be read-only or locked by the editor.")));
  }

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("sourceDirectory"), ContentDir);
  Result->SetStringField(TEXT("destinationPath"), DestinationPath);
  Result->SetNumberField(TEXT("copiedCount"), Copied);
  Result->SetNumberField(TEXT("skippedCount"), Skipped);
  Result->SetNumberField(TEXT("failedCount"), Failed);
  Result->SetNumberField(TEXT("totalFiles"), RelativeFiles.Num());
  Result->SetBoolField(TEXT("dryRun"), bDryRun);
  Result->SetStringField(TEXT("referenceIntegrity"),
                         bStructurePreserved ? TEXT("preserved") : TEXT("at-risk"));
  Result->SetArrayField(TEXT("packagePaths"), Sample);
  Result->SetArrayField(TEXT("failedFiles"), Failures);
  Result->SetArrayField(TEXT("warnings"), Warnings);
  SendAutomationResponse(
      Socket, RequestId, Failed == 0,
      FString::Printf(TEXT("%s %d file(s) into %s (%d skipped, %d failed)."),
                      bDryRun ? TEXT("Would migrate") : TEXT("Migrated"), Copied,
                      *DestinationPath, Skipped, Failed),
      Result, Failed == 0 ? TEXT("") : TEXT("PARTIAL_FAILURE"));
  return true;
}
#else
bool UMcpAutomationBridgeSubsystem::HandleMigrateAssets(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("migrate_assets requires the editor."), nullptr,
                         TEXT("EDITOR_ONLY"));
  return true;
}
#endif

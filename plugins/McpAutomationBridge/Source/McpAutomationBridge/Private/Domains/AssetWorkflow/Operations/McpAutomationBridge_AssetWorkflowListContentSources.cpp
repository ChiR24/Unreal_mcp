// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Domains/AssetWorkflow/Operations/McpAutomationBridge_AssetWorkflowContentSourceRoots.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
namespace
{
/** One candidate source, resolved cheaply. Package counting happens per page. */
struct FContentSourceEntry
{
	FString Root;
	FString Id;
	FString Kind;
	FString Dir;
	bool bIsFeaturePack = false;
};

int32 CountPackages(const FString& Dir)
{
	TArray<FString> Found;
	IFileManager::Get().FindFilesRecursive(Found, *Dir, TEXT("*.uasset"), true, false, false);
	const int32 Assets = Found.Num();
	Found.Reset();
	IFileManager::Get().FindFilesRecursive(Found, *Dir, TEXT("*.umap"), true, false, false);
	return Assets + Found.Num();
}

TSharedPtr<FJsonObject> DescribeEntry(const FContentSourceEntry& Entry, bool bIncludeCounts)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("sourceRoot"), Entry.Root);
	Json->SetStringField(TEXT("sourceId"), Entry.Id);
	Json->SetStringField(TEXT("kind"), Entry.Kind);
	if (Entry.bIsFeaturePack)
	{
		Json->SetBoolField(TEXT("hasContentFolder"), false);
		// .upack is a packed archive, not a loose content tree, so the
		// copy-based migrate path cannot consume it.
		Json->SetBoolField(TEXT("migratable"), false);
		Json->SetStringField(TEXT("note"), TEXT("Packed archive; add via the editor's Add Feature Pack, not migrate_assets."));
		return Json;
	}
	const FString ContentDir = McpContentSources::ContentDirFor(Entry.Dir);
	Json->SetBoolField(TEXT("hasContentFolder"), ContentDir != Entry.Dir);
	// A pack is migratable when it actually holds packages; empty scaffolding
	// directories are listed but flagged, so a caller does not burn a migrate
	// call discovering there was nothing to copy.
	const int32 PackageCount = CountPackages(ContentDir);
	Json->SetBoolField(TEXT("migratable"), PackageCount > 0);
	if (bIncludeCounts)
	{
		Json->SetNumberField(TEXT("packageCount"), PackageCount);
	}
	return Json;
}

void CollectSubdirectories(const FString& RootDir, TArray<FString>& OutNames)
{
	IFileManager::Get().FindFiles(OutNames, *(RootDir / TEXT("*")), false, true);
	OutNames.Sort();
}

/** Plugin roots nest one level (Runtime/, Editor/, ...); recurse a single step. */
void CollectPluginEntries(const FString& Root, const FString& RootDir, const FString& Name,
                          const FString& NameFilter, TArray<FContentSourceEntry>& Out)
{
	const FString Dir = RootDir / Name;
	TArray<FString> UPlugins;
	IFileManager::Get().FindFiles(UPlugins, *(Dir / TEXT("*.uplugin")), true, false);
	if (UPlugins.Num() > 0)
	{
		Out.Add({Root, Name, TEXT("plugin"), Dir, false});
		return;
	}
	TArray<FString> Nested;
	CollectSubdirectories(Dir, Nested);
	for (const FString& Inner : Nested)
	{
		TArray<FString> InnerPlugins;
		IFileManager::Get().FindFiles(InnerPlugins, *(Dir / Inner / TEXT("*.uplugin")), true, false);
		const FString Id = Name / Inner;
		if (InnerPlugins.Num() > 0 && (NameFilter.IsEmpty() || Id.Contains(NameFilter)))
		{
			Out.Add({Root, Id, TEXT("plugin"), Dir / Inner, false});
		}
	}
}
} // namespace

bool UMcpAutomationBridgeSubsystem::HandleListContentSources(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  FString RootFilter;
  Payload->TryGetStringField(TEXT("sourceRoot"), RootFilter);
  bool bIncludeCounts = false;
  Payload->TryGetBoolField(TEXT("includePackageCounts"), bIncludeCounts);
  FString NameFilter;
  Payload->TryGetStringField(TEXT("filter"), NameFilter);

  TArray<FString> Roots;
  if (RootFilter.IsEmpty()) {
    Roots = McpContentSources::AllRootTokens();
  } else if (McpContentSources::ResolveRootDir(RootFilter).IsEmpty()) {
    SendAutomationResponse(
        Socket, RequestId, false,
        FString::Printf(TEXT("Unknown sourceRoot '%s'. Allowed: %s"), *RootFilter,
                        *FString::Join(McpContentSources::AllRootTokens(), TEXT(", "))),
        nullptr, TEXT("INVALID_ARGUMENT"));
    return true;
  } else {
    Roots.Add(RootFilter);
  }

  TArray<FContentSourceEntry> Entries;
  TSharedPtr<FJsonObject> RootDirs = MakeShared<FJsonObject>();
  TArray<TSharedPtr<FJsonValue>> MissingRoots;

  for (const FString &Root : Roots) {
    const FString RootDir = McpContentSources::ResolveRootDir(Root);
    RootDirs->SetStringField(Root, RootDir);
    if (!IFileManager::Get().DirectoryExists(*RootDir)) {
      MissingRoots.Add(MakeShared<FJsonValueString>(Root));
      continue;
    }

    if (Root == TEXT("engineFeaturePacks")) {
      TArray<FString> Packs;
      IFileManager::Get().FindFiles(Packs, *(RootDir / TEXT("*.upack")), true, false);
      Packs.Sort();
      for (const FString &Pack : Packs) {
        if (NameFilter.IsEmpty() || Pack.Contains(NameFilter)) {
          Entries.Add({Root, Pack, TEXT("featurePack"), RootDir / Pack, true});
        }
      }
      continue;
    }

    const bool bIsPluginRoot =
        Root == TEXT("enginePlugins") || Root == TEXT("projectPlugins");
    const FString Kind = Root == TEXT("engineTemplates")    ? TEXT("template")
                         : Root == TEXT("megascansLibrary") ? TEXT("megascansPack")
                         : Root == TEXT("fabLibrary")       ? TEXT("fabPack")
                                                            : TEXT("contentFolder");

    TArray<FString> Names;
    CollectSubdirectories(RootDir, Names);
    for (const FString &Name : Names) {
      if (bIsPluginRoot) {
        CollectPluginEntries(Root, RootDir, Name, NameFilter, Entries);
        continue;
      }
      if (NameFilter.IsEmpty() || Name.Contains(NameFilter)) {
        Entries.Add({Root, Name, Kind, RootDir / Name, false});
      }
    }
  }

  // Bounded page. An unfiltered sweep of enginePlugins alone finds ~650
  // entries, and describing all of them recursively counts packages under every
  // one — a default call has to stay cheap in both bytes and disk work, so only
  // the page that is returned is described.
  double Limit = 50;
  Payload->TryGetNumberField(TEXT("limit"), Limit);
  const int32 PageSize = FMath::Clamp(static_cast<int32>(Limit), 1, 500);
  double Offset = 0;
  Payload->TryGetNumberField(TEXT("offset"), Offset);
  const int32 Start = FMath::Clamp(static_cast<int32>(Offset), 0, Entries.Num());
  const int32 End = FMath::Min(Start + PageSize, Entries.Num());

  TArray<TSharedPtr<FJsonValue>> Sources;
  for (int32 Index = Start; Index < End; ++Index) {
    Sources.Add(MakeShared<FJsonValueObject>(DescribeEntry(Entries[Index], bIncludeCounts)));
  }

  const bool bHasMore = End < Entries.Num();
  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetArrayField(TEXT("sources"), Sources);
  Result->SetNumberField(TEXT("sourceCount"), Sources.Num());
  Result->SetNumberField(TEXT("totalCount"), Entries.Num());
  Result->SetNumberField(TEXT("limit"), PageSize);
  Result->SetNumberField(TEXT("offset"), Start);
  Result->SetBoolField(TEXT("hasMore"), bHasMore);
  Result->SetNumberField(TEXT("nextOffset"), bHasMore ? End : -1);
  Result->SetObjectField(TEXT("rootDirectories"), RootDirs);
  Result->SetArrayField(TEXT("missingRoots"), MissingRoots);
  SendAutomationResponse(
      Socket, RequestId, true,
      FString::Printf(TEXT("Returned %d of %d content source(s) across %d root(s)."),
                      Sources.Num(), Entries.Num(), Roots.Num()),
      Result);
  return true;
}
#else
bool UMcpAutomationBridgeSubsystem::HandleListContentSources(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  SendAutomationResponse(Socket, RequestId, false,
                         TEXT("list_content_sources requires the editor."), nullptr,
                         TEXT("EDITOR_ONLY"));
  return true;
}
#endif

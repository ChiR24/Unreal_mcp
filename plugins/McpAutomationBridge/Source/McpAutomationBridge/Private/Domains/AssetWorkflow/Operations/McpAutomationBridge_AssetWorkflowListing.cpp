// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"
#include "Misc/EngineVersionComparison.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Foundation/BridgeHelpers/Assets/McpAutomationBridgeHelpersAssetResolution.h"
#include "Domains/AssetWorkflow/Operations/McpAutomationBridgeAssetListCursor.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleListAssets(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
#if WITH_EDITOR
  // Parse filters
  FString PathFilter;
  FString ClassFilter;
  FString TagFilter;
  FString PathStartsWith;

  const TSharedPtr<FJsonObject> *FilterObj;
  if (Payload->TryGetObjectField(TEXT("filter"), FilterObj) && FilterObj) {
    (*FilterObj)->TryGetStringField(TEXT("path"), PathFilter);
    (*FilterObj)->TryGetStringField(TEXT("class"), ClassFilter);
    (*FilterObj)->TryGetStringField(TEXT("tag"), TagFilter);
    (*FilterObj)->TryGetStringField(TEXT("pathStartsWith"), PathStartsWith);
  } else {
    // Legacy support for direct path/recursive fields
    Payload->TryGetStringField(TEXT("path"), PathFilter);
  }

  // Canonicalize + validate the listing path. Blocks traversal and invalid
  // roots; TS already canonicalizes but native MCP callers may send raw input.
  FString RawPath = PathFilter.IsEmpty() ? TEXT("/Game") : PathFilter;
  FNormalizedAssetPath NormPath = NormalizeAssetPath(RawPath);
  if (!NormPath.bIsValid) {
    SendAutomationError(Socket, RequestId, NormPath.ErrorMessage, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  PathFilter = NormPath.Path;

  if (!PathStartsWith.IsEmpty()) {
    FNormalizedAssetPath NormStart = NormalizeAssetPath(PathStartsWith);
    if (NormStart.bIsValid) {
      PathStartsWith = NormStart.Path;
    }
  }

  bool bRecursive = true;
  Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

  // Parse bounds: direct limit/offset or pagination.{limit,offset}.
  int32 Offset = 0;
  int32 Limit = 50;
  const TSharedPtr<FJsonObject> *PaginationObj;
  if (Payload->TryGetObjectField(TEXT("pagination"), PaginationObj) && PaginationObj) {
    (*PaginationObj)->TryGetNumberField(TEXT("offset"), Offset);
    (*PaginationObj)->TryGetNumberField(TEXT("limit"), Limit);
  }
  Payload->TryGetNumberField(TEXT("limit"), Limit);
  Payload->TryGetNumberField(TEXT("offset"), Offset);

  // Opaque cursor overrides offset and is validated for path containment
  // (TOCTOU) and the per-session catalog revision (stale / cross-session).
  FString CursorStr;
  Payload->TryGetStringField(TEXT("cursor"), CursorStr);
  if (!CursorStr.IsEmpty()) {
    FMcpAssetListCursor Cursor;
    if (!McpDecodeAssetListCursor(CursorStr, Cursor)) {
      SendAutomationError(Socket, RequestId, TEXT("Invalid pagination cursor"), TEXT("INVALID_CURSOR"));
      return true;
    }
    if (Cursor.Revision != McpGetAssetListRevision()) {
      SendAutomationError(Socket, RequestId, TEXT("Pagination cursor is stale (catalog revision changed). Restart the listing."), TEXT("STALE_CURSOR"));
      return true;
    }
    if (Cursor.Path != PathFilter) {
      SendAutomationError(Socket, RequestId, TEXT("Pagination cursor path does not match the requested path."), TEXT("STALE_CURSOR"));
      return true;
    }
    Offset = Cursor.Offset;
  }

  // Validated bounded limits: hard max 500, min 1, non-negative offset.
  const int32 MaxLimit = 500;
  Limit = FMath::Clamp(Limit, 1, MaxLimit);
  Offset = FMath::Max(0, Offset);

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  FARFilter Filter;
  Filter.bRecursivePaths = bRecursive;
  Filter.bRecursiveClasses = true;

  // Apply path filters
  if (!PathFilter.IsEmpty()) {
    Filter.PackagePaths.Add(FName(*PathFilter));
  } else if (!PathStartsWith.IsEmpty()) {
    // If we have a path prefix, assume it's a package path
    // Note: FARFilter doesn't support 'StartsWith' natively for paths in an
    // efficient way other than adding the path and set bRecursivePaths=true. So
    // if PathStartsWith is a folder, we use it.
    Filter.PackagePaths.Add(FName(*PathStartsWith));
  } else {
    // Default to /Game to prevent empty results or massive scan
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
  }

  // Use cached AssetRegistry data — ScanPathsSynchronous() removed to prevent
  // blocking the GameThread (causes SSE/HTTP transport timeouts).
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.

  if (!ClassFilter.IsEmpty()) {
    // Support both short class names and full paths (best effort)
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    FTopLevelAssetPath ClassPath(ClassFilter);
    if (ClassPath.IsValid()) {
      Filter.ClassPaths.Add(ClassPath);
    }
#else
    // UE 5.0: Use ClassNames instead of ClassPaths
    Filter.ClassNames.Add(FName(*ClassFilter));
#endif
  }

  // Tags are not standard on assets in the same way as actors.
  // AssetRegistry tags are Key-Value pairs.
  // If TagFilter is provided, we assume it checks for the existence of a tag
  // key or value. Implementing a generic "HasTag" is ambiguous. We'll assume
  // TagFilter refers to a metadata key presence.

  // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
  // Asset listing uses cached AssetRegistry data exclusively.
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan" or rescan_content_directory.
  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);

  // Post-filtering
  if (!ClassFilter.IsEmpty() || !TagFilter.IsEmpty()) {
    AssetList.RemoveAll([&](const FAssetData &Asset) {
      if (!ClassFilter.IsEmpty()) {
        // Check full class path or asset class name
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        FString AssetClass = Asset.AssetClassPath.ToString();
        FString AssetClassName = Asset.AssetClassPath.GetAssetName().ToString();
#else
        FString AssetClass = Asset.AssetClass.ToString();
        FString AssetClassName = Asset.AssetClass.ToString();
#endif
        if (!AssetClass.Equals(ClassFilter) &&
            !AssetClassName.Equals(ClassFilter)) {
          return true; // Remove
        }
      }
      if (!TagFilter.IsEmpty()) {
        if (!Asset.TagsAndValues.Contains(FName(*TagFilter))) {
          return true; // Remove
        }
      }
      return false;
    });
  }

  // Filter by Depth if specified
  // (Changes made to support depth and folders - Touch to force rebuild)
  int32 Depth = -1;
  Payload->TryGetNumberField(TEXT("depth"), Depth);

  if (Depth >= 0 && bRecursive && !PathFilter.IsEmpty()) {
    // Normalize base path for depth calculation
    FString BasePath = PathFilter;
    if (BasePath.EndsWith(TEXT("/"))) {
      BasePath.RemoveAt(BasePath.Len() - 1);
    }
    // Base depth: number of slashes in /Game/Foo is 2
    int32 BaseSlashCount = 0;
    for (const TCHAR *P = *BasePath; *P; ++P) {
      if (*P == TEXT('/'))
        BaseSlashCount++;
    }

    AssetList.RemoveAll([&](const FAssetData &Asset) {
      FString PkgPath = Asset.PackagePath.ToString();
      // If PkgPath is shorter than BasePath (shouldn't happen with filter),
      // keep it I guess? Actually we only care about descendants.

      int32 SlashCount = 0;
      for (const TCHAR *P = *PkgPath; *P; ++P) {
        if (*P == TEXT('/'))
          SlashCount++;
      }

      // Difference in slashes determines depth
      // /Game (1 slash) vs /Game/A (2 slashes) -> Diff 1 -> Depth 0 (immediate
      // child) Wait, PackagePath for /Game/A is /Game. PackagePath for
      // /Game/Sub/B is /Game/Sub.

      // Let's test:
      // Filter: /Game (Slash=1)
      // Asset: /Game/A (PackagePath=/Game, Slash=1). Diff=0. Depth 0? Yes.
      // Asset: /Game/Sub/B (PackagePath=/Game/Sub, Slash=2). Diff=1. Depth 1?
      // Yes.

      // If Depth=0, we want Diff=0.
      // If Depth=1, we want Diff<=1.

      return (SlashCount - BaseSlashCount) > Depth;
    });
  }

  // Deterministic, stable ordering before pagination (package path, then name).
  AssetList.Sort([](const FAssetData &A, const FAssetData &B) {
    const int32 PkgCmp = A.PackagePath.ToString().Compare(B.PackagePath.ToString());
    if (PkgCmp != 0) return PkgCmp < 0;
    return A.AssetName.ToString() < B.AssetName.ToString();
  });

  const int32 TotalCount = AssetList.Num();

  // Apply pagination as a stable slice of the sorted list.
  TArray<FAssetData> Page;
  if (TotalCount > 0 && Offset < TotalCount) {
    const int32 Count = FMath::Min(Limit, TotalCount - Offset);
    Page.Append(AssetList.GetData() + Offset, Count);
  }

  // Also fetch sub-folders if we are listing a directory (PathFilter is set)
  TArray<FString> SubPathList;
  if (!PathFilter.IsEmpty()) {
    // If non-recursive (or depth limited), we generally want at least the
    // immediate subfolders. GetSubPaths is non-recursive by default.
    AssetRegistry.GetSubPaths(PathFilter, SubPathList, false);

    // If Depth is specified, we might want deeper folders?
    // Actually, standard 'ls' behavior on a folder shows immediate children
    // (files and folders). If recursive, it shows everything. Let keeps it
    // simple: If we are listing a path, show its immediate subfolders. Getting
    // ALL recursive folders might be too much info if strictly not requested,
    // but 'GetSubPaths' with bInRecurse=true gets everything.

    // Decision:
    // If Recursive=true (and Depth not limited), maybe we don't strictly need
    // folders as assets cover it? But user asked for folders when assets are
    // missing. Default 'ls' shows immediate folders. So let's always include
    // immediate subfolders of the requested path.
  }

  const bool bIncludeTags = Payload->HasField(TEXT("includeTags"))
    ? Payload->GetBoolField(TEXT("includeTags"))
    : false;

  TArray<TSharedPtr<FJsonValue>> AssetsArray;
  for (const FAssetData &Asset : Page) {
    TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
    AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    AssetObj->SetStringField(TEXT("path"), Asset.GetSoftObjectPath().ToString());
    AssetObj->SetStringField(TEXT("class"), Asset.AssetClassPath.ToString());
#else
    AssetObj->SetStringField(TEXT("path"), Asset.ToSoftObjectPath().ToString());
    AssetObj->SetStringField(TEXT("class"), Asset.AssetClass.ToString());
#endif
    AssetObj->SetStringField(TEXT("packagePath"), Asset.PackagePath.ToString());

    // Add tags only when explicitly requested (off by default).
    if (bIncludeTags) {
      TArray<TSharedPtr<FJsonValue>> Tags;
      for (auto TagPair : Asset.TagsAndValues) {
        Tags.Add(MakeShared<FJsonValueString>(TagPair.Key.ToString()));
      }
      AssetObj->SetArrayField(TEXT("tags"), Tags);
    }

    AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
  }

  TArray<TSharedPtr<FJsonValue>> FoldersJson;
  for (const FString &SubPath : SubPathList) {
    FoldersJson.Add(MakeShared<FJsonValueString>(SubPath));
  }

  const bool bHasMore = (Offset + Page.Num()) < TotalCount;
  const int32 NextOffset = Offset + Page.Num();
  const FString CurrentCursor = McpEncodeAssetListCursor({ Offset, PathFilter, McpGetAssetListRevision() });
  const FString NextCursor = bHasMore
    ? McpEncodeAssetListCursor({ NextOffset, PathFilter, McpGetAssetListRevision() })
    : FString();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
  Resp->SetBoolField(TEXT("success"), true);
  Resp->SetArrayField(TEXT("assets"), AssetsArray);
  Resp->SetArrayField(TEXT("folders"), FoldersJson);
  Resp->SetNumberField(TEXT("totalCount"), TotalCount);
  Resp->SetNumberField(TEXT("count"), AssetsArray.Num());
  Resp->SetNumberField(TEXT("limit"), Limit);
  Resp->SetNumberField(TEXT("offset"), Offset);
  Resp->SetBoolField(TEXT("hasMore"), bHasMore);
  Resp->SetNumberField(TEXT("nextOffset"), NextOffset);
  Resp->SetStringField(TEXT("cursor"), CurrentCursor);
  Resp->SetStringField(TEXT("nextCursor"), NextCursor);

  SendAutomationResponse(Socket, RequestId, true, TEXT("Assets listed"), Resp,
                         FString());
  return true;
#else
  SendAutomationError(Socket, RequestId, TEXT("Editor build required"), TEXT("NOT_SUPPORTED"));
  return true;
#endif
}

/**
 * Handles requests to get detailed information about a single asset.
 *
 * @param RequestId Unique request identifier.
 * @param Payload JSON payload containing 'assetPath'.
 * @param Socket WebSocket connection.
 * @return True if handled.
 */

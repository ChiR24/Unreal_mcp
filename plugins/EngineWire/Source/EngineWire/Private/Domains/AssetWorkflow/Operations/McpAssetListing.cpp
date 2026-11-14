// Copyright (c) 2024 EngineWire Contributors

#include "EngineWireSubsystem.h"
#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#include "Dom/JsonObject.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Foundation/BridgeHelpers/Assets/McpAssetResolve.h"
#include "Domains/AssetWorkflow/Operations/McpAssetListCursor.h"

bool UEngineWireSubsystem::HandleListAssets(
    const FString &RequestId, const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket) {
  // path is the folder listed (default /Game); filter narrows by asset name (case-insensitive substring).
  FString PathFilter;
  const bool bHasExplicitPath = Payload->TryGetStringField(TEXT("path"), PathFilter);
  FString NameFilter;
  Payload->TryGetStringField(TEXT("filter"), NameFilter);

  // Canonicalize + validate the listing path. Blocks traversal and invalid
  // roots: callers may send raw input.
  FString RawPath = PathFilter.IsEmpty() ? TEXT("/Game") : PathFilter;
  FNormalizedAssetPath NormPath = NormalizeAssetPath(RawPath);
  if (!NormPath.bIsValid) {
    SendAutomationError(Socket, RequestId, NormPath.ErrorMessage, TEXT("INVALID_ARGUMENT"));
    return true;
  }
  PathFilter = NormPath.Path;
  const FString ExplicitListingPath = PathFilter;

  bool bRecursive = true;
  Payload->TryGetBoolField(TEXT("recursive"), bRecursive);

  // Opt-in per-asset disk metadata (size + modified date). Off by default:
  // it costs one file stat per listed asset and grows the response.
  bool bIncludeMetadata = false;
  Payload->TryGetBoolField(TEXT("includeMetadata"), bIncludeMetadata);

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

  // Opaque cursor overrides offset and the filtering path (its embedded path
  // is adopted for filtering) and is validated for the per-session catalog
  // revision (stale / cross-session). When the caller also supplies an explicit
  // listing path, the two must agree: the cursor is the single source of truth
  // for continuation, so a mismatched explicit path is a typed conflict.
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
    FNormalizedAssetPath NormCursorPath = NormalizeAssetPath(Cursor.Path);
    if (!NormCursorPath.bIsValid) {
      SendAutomationError(Socket, RequestId, TEXT("Cursor embeds an invalid listing path"), TEXT("INVALID_CURSOR"));
      return true;
    }
    // A caller that paginates with only the cursor (no explicit path) keeps the
    // cursor's embedded path so page 2+ stays consistent. A caller that also
    // passes an explicit path must keep it identical to the cursor path.
    if (bHasExplicitPath && ExplicitListingPath != NormCursorPath.Path) {
      SendAutomationError(
        Socket, RequestId,
        FString::Printf(TEXT("Explicit path '%s' conflicts with continuation cursor path '%s'"),
          *ExplicitListingPath, *NormCursorPath.Path),
        TEXT("CURSOR_CONTEXT_MISMATCH"));
      return true;
    }
    PathFilter = NormCursorPath.Path;
    Offset = Cursor.Offset;
  }

  // Validated bounded limits: hard max 500, min 1, non-negative offset.
  const int32 MaxLimit = 500;
  Limit = FMath::Clamp(Limit, 1, MaxLimit);
  Offset = FMath::Max(0, Offset);

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  IAssetRegistry &AssetRegistry = AssetRegistryModule.Get();

  // PathFilter is never empty here: a valid NormalizeAssetPath result is a long package name.
  FARFilter Filter;
  Filter.bRecursivePaths = bRecursive;
  Filter.PackagePaths.Add(FName(*PathFilter));

  // NOTE: ScanPathsSynchronous() was removed to prevent GameThread blocking.
  // Asset listing uses cached AssetRegistry data exclusively.
  // LIMITATION: Assets not yet indexed by the editor's background scanner
  // will NOT appear. Use Content Browser "Rescan".
  TArray<FAssetData> AssetList;
  AssetRegistry.GetAssets(Filter, AssetList);
  if (!NameFilter.IsEmpty()) {
    AssetList.RemoveAll([&](const FAssetData &Asset) {
      return !Asset.AssetName.ToString().Contains(NameFilter, ESearchCase::IgnoreCase);
    });
  }

  // depth N keeps assets at most N folders below the filter path: an asset's
  // PackagePath is its folder, so /Game/A sits at depth 0 under /Game.
  int32 Depth = -1;
  Payload->TryGetNumberField(TEXT("depth"), Depth);
  const auto CountSlashes = [](const FString &Value) { int32 Count = 0; for (TCHAR C : Value) { if (C == TEXT('/')) { ++Count; } } return Count; };

  if (Depth >= 0 && bRecursive) {
    FString BasePath = PathFilter;
    BasePath.RemoveFromEnd(TEXT("/"));
    const int32 BaseSlashCount = CountSlashes(BasePath);
    AssetList.RemoveAll([&](const FAssetData &Asset) {
      return CountSlashes(Asset.PackagePath.ToString()) - BaseSlashCount > Depth;
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

  // Also fetch the sub-folders: the immediate ones, and for a recursive listing the nested ones, trimmed by depth.
  TArray<FString> SubPathList;
  AssetRegistry.GetSubPaths(PathFilter, SubPathList, bRecursive);
  const int32 BaseSlashes = CountSlashes(PathFilter);
  SubPathList.RemoveAll([&](const FString &Sub) { return Depth >= 0 && CountSlashes(Sub) - BaseSlashes - 1 > Depth; });

  bool bIncludeTags = false;
  Payload->TryGetBoolField(TEXT("includeTags"), bIncludeTags);

  // File stats below are synchronous game-thread IO. Bound how many assets we
  // stat so an unbounded recursive listing (no pagination limit) cannot stall
  // the editor. Assets past the cap are still listed, just without disk
  // metadata; the caller is told via metadataTruncated.
  const int32 MetadataStatCap = 256;
  int32 MetadataStatBudget = bIncludeMetadata ? MetadataStatCap : 0;
  bool bMetadataTruncated = false;

  TArray<TSharedPtr<FJsonValue>> AssetsArray;
  for (const FAssetData &Asset : Page) {
    TSharedPtr<FJsonObject> AssetObj = McpHandlerUtils::CreateResultObject();
    AssetObj->SetStringField(TEXT("name"), Asset.AssetName.ToString());
    AssetObj->SetStringField(TEXT("path"), MCP_ASSET_DATA_GET_SOFT_PATH(Asset));
    AssetObj->SetStringField(TEXT("class"), MCP_ASSET_DATA_GET_CLASS_PATH(Asset));
    AssetObj->SetStringField(TEXT("packagePath"), Asset.PackagePath.ToString());

    // Add tags only when explicitly requested (off by default).
    if (bIncludeTags) {
      TArray<TSharedPtr<FJsonValue>> Tags;
      for (auto TagPair : Asset.TagsAndValues) {
        Tags.Add(MakeShared<FJsonValueString>(TagPair.Key.ToString()));
      }
      AssetObj->SetArrayField(TEXT("tags"), Tags);
    }

    if (bIncludeMetadata) {
      if (MetadataStatBudget > 0) {
        --MetadataStatBudget;
        FString PackageFilename;
        if (FPackageName::DoesPackageExist(Asset.PackageName.ToString(),
                                           &PackageFilename)) {
          IFileManager &FileManager = IFileManager::Get();
          const int64 FileSize = FileManager.FileSize(*PackageFilename);
          if (FileSize >= 0) {
            AssetObj->SetNumberField(TEXT("diskSizeBytes"),
                                     static_cast<double>(FileSize));
          }
          const FDateTime Stamp = FileManager.GetTimeStamp(*PackageFilename);
          if (Stamp != FDateTime::MinValue()) {
            AssetObj->SetStringField(TEXT("modifiedAt"), Stamp.ToIso8601());
          }
        }
      } else {
        bMetadataTruncated = true;
      }
    }

    AssetsArray.Add(MakeShared<FJsonValueObject>(AssetObj));
  }

  TArray<TSharedPtr<FJsonValue>> FoldersJson;
  for (const FString &SubPath : SubPathList) {
    // A name filter narrows the folders as it does the assets: every subfolder of /Game buried five matches.
    if (NameFilter.IsEmpty() || FPaths::GetCleanFilename(SubPath).Contains(NameFilter, ESearchCase::IgnoreCase)) {
      FoldersJson.Add(MakeShared<FJsonValueString>(SubPath));
    }
  }

  const bool bHasMore = (Offset + Page.Num()) < TotalCount;
  const int32 NextOffset = Offset + Page.Num();
  const FString CurrentCursor = McpEncodeAssetListCursor({ Offset, PathFilter, McpGetAssetListRevision() });
  const FString NextCursor = bHasMore
    ? McpEncodeAssetListCursor({ NextOffset, PathFilter, McpGetAssetListRevision() })
    : FString();

  TSharedPtr<FJsonObject> Resp = McpHandlerUtils::CreateResultObject();
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
  if (bIncludeMetadata) {
    Resp->SetBoolField(TEXT("metadataTruncated"), bMetadataTruncated);
  }

  SendAutomationResponse(Socket, RequestId, true, TEXT("Assets listed"), Resp,
                         FString());
  return true;
}


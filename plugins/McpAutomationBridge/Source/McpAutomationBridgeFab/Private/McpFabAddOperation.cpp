// Copyright (c) 2024 MCP Automation Bridge Contributors

// Turning "Fab accepted the workflow" into "content exists in this project".
//
// AddToProject's JS reply only means the page handed a URL to Fab's importer.
// The download and the import happen afterwards, asynchronously, and can fail
// or stall. Treating the reply as success would report a finished operation
// while the Content folder was still empty -- the exact mistake that made an
// earlier verification pass claim nothing had been imported when 188 assets
// were landing one directory over.
//
// So completion is observed from Unreal: watch the asset registry for packages
// that did not exist when the run started, wait for the stream to go quiet,
// then report what actually appeared.
//
// That watch runs for minutes and, once Fab's importer starts, holds the game
// thread, so nobody waits on it: the caller is answered when Fab accepts the
// download, and the outcome is kept under an operation id for a status read.

#include "McpFabProvider.h"
#include "McpFabAddScript.h"
#include "McpFabBridgeDispatch.h"
#include "McpFabImportOperations.h"
#include "McpFabImportWatcher.h"
#include "McpFabInterchange.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Runtime/Launch/Resources/Version.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpFabAddOp, Log, All);

namespace McpFabAddOperation
{
namespace
{
/** Every /Game package path the registry knows right now. */
TSet<FString> SnapshotGameAssets()
{
	TSet<FString> Paths;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
		TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(TEXT("/Game")), Assets, /*bRecursive=*/true);
	for (const FAssetData& Asset : Assets)
	{
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
		Paths.Add(Asset.GetObjectPathString());
#else
		Paths.Add(Asset.ObjectPath.ToString());
#endif
	}
	return Paths;
}

/** Names the import that is in the way, so a refused caller knows what to wait for and where to look. */
FString DescribeRunning(const FMcpFabImportStatus& Running)
{
	return FString::Printf(
		TEXT("Fab is still importing listing %s (operation %s, %s, %.0f s in). Poll asset.query_marketplace with lookup=fab_import_status and operationId %s until its phase is done or failed, then add the next listing."),
		*Running.ListingId, *Running.OperationId, *Running.Phase, Running.ElapsedSeconds, *Running.OperationId);
}

/** What the add told the caller, read out of the page's reply. Parsed even on failure: the page says why it refused. */
FMcpFabAddResult ParseAddReply(bool bSuccess, const FString& Payload)
{
	FMcpFabAddResult Result;
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
	// Gating the parse on bSuccess turned every refusal into a bare
	// FAB_REJECTED that named nothing.
	const bool bJson = FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid();
	if (bJson) { Root->TryGetStringField(TEXT("error"), Result.ErrorCode); }
	if (bSuccess && bJson)
	{
		Root->TryGetBoolField(TEXT("accepted"), Result.bAccepted);
		Root->TryGetBoolField(TEXT("engineExactMatch"), Result.bEngineExactMatch);
		Root->TryGetStringField(TEXT("versionName"), Result.VersionName);
		Root->TryGetStringField(TEXT("formatCode"), Result.FormatCode);
		Root->TryGetStringField(TEXT("quality"), Result.Quality);
		Root->TryGetBoolField(TEXT("combinesMeshes"), Result.bMergesMeshes);
		double Bytes = 0.0;
		if (Root->TryGetNumberField(TEXT("downloadBytes"), Bytes) && Bytes >= 0.0)
		{
			Result.DownloadBytes = static_cast<int64>(Bytes);
		}
		// A pack's version name is not a file; only a source format downloads one by that name.
		if (Result.FormatCode != TEXT("unreal-engine")) { Result.FileName = Result.VersionName; }
	}
	if (Result.bAccepted)
	{
		return Result;
	}
	// Safe to log in full: the page reports codes, statuses and key shapes, never values --
	// shape() emits key names and types, and the download url stays a local in the script.
	// Without this the self-diagnosing fields the script already computes never reach
	// anywhere a human can read them.
	UE_LOG(LogMcpFabAddOp, Warning, TEXT("Fab refused the add; page reported: %s"), *Payload);
	if (Result.ErrorCode.IsEmpty()) { Result.ErrorCode = TEXT("FAB_REJECTED"); }

	// The page already reports the formats it saw; naming them turns
	// NO_IMPORTABLE_FORMAT from "it failed somewhere" into "this
	// listing ships these formats and Fab imports none of them",
	// which is the difference between a dead end and a next step.
	TArray<FString> Formats;
	const TArray<TSharedPtr<FJsonValue>>* FormatRows = nullptr;
	if (bJson && Root->TryGetArrayField(TEXT("formatCodes"), FormatRows) && FormatRows != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& Row : *FormatRows)
		{
			FString Code;
			if (Row.IsValid() && Row->TryGetString(Code) && !Code.IsEmpty())
			{
				Formats.Add(Code);
			}
		}
	}
	// The message follows the code. Claiming "none importable" for a
	// NO_VERSION failure described the wrong step and sent the reader
	// looking at the format list, which was fine.
	const FString FormatList = Formats.Num() > 0
		? FString::Printf(TEXT(" It advertises: %s."), *FString::Join(Formats, TEXT(", ")))
		: FString();
	if (Result.ErrorCode == TEXT("NO_IMPORTABLE_FORMAT"))
	{
		Result.Error = FString::Printf(
			TEXT("Fab did not accept the listing.%s Fab imports unreal-engine, gltf, glb and fbx; this listing ships none of them."),
			*FormatList);
	}
	else if (Result.ErrorCode == TEXT("LARGE_SCENE_FILE"))
	{
		Result.Error = FString::Printf(
			TEXT("This listing ships one %s file of %.0f MB. Fab's importer merges every mesh in a file into a single static mesh, so a scene that size becomes one unplaceable mesh that can need gigabytes to build and holds the editor for many minutes. Nothing was downloaded. Pass combineMeshes=false to import each mesh separately, or combineMeshes=true to accept the single merged mesh."),
			*Result.FormatCode, static_cast<double>(Result.DownloadBytes) / 1.0e6);
	}
	else if (Result.ErrorCode == TEXT("NO_VERSION"))
	{
		Result.Error = FString::Printf(
			TEXT("A format was selected but it published no downloadable version.%s See versionShape in the log for what the asset-formats response actually contained."),
			*FormatList);
	}
	else
	{
		Result.Error = FString::Printf(
			TEXT("Fab did not accept the listing (%s).%s"),
			*Result.ErrorCode, *FormatList);
	}
	return Result;
}
} // namespace

/** Shared entry point used by the provider implementation. */
bool Start(const FString& ListingId, const FString& EngineVersion, const FString& CacheLocation,
	const FMcpFabAddOptions& Options, TFunction<void(const FMcpFabAddResult&)> OnAccepted)
{
	// Fail closed where ListingId is interpolated into a JS string literal inside
	// Fab's authenticated page: an id that cannot reach the page cannot steer the path.
	if (!IsSafeListingId(ListingId))
	{
		FMcpFabAddResult Rejected;
		Rejected.ErrorCode = TEXT("INVALID_LISTING_ID");
		Rejected.Error = TEXT("A listing id must be [A-Za-z0-9_-] and at most 64 characters.");
		OnAccepted(Rejected);
		return true;
	}

	// Asking for separate meshes when the engine gives no way to switch merging off would fuse the
	// import anyway; refusing before anything is claimed or downloaded is the honest answer.
	if (Options.CombineMeshes.IsSet() && !Options.CombineMeshes.GetValue() && !McpFabInterchange::CanSeparateMeshes())
	{
		FMcpFabAddResult Unsupported;
		Unsupported.ErrorCode = TEXT("COMBINE_UNSUPPORTED");
		Unsupported.Error = TEXT("This engine's Interchange has no mesh-combining setting the adapter can reach, so it cannot import meshes separately. Omit combineMeshes, or pass combineMeshes=true to accept Fab's single merged mesh.");
		OnAccepted(Unsupported);
		return true;
	}

	// One import at a time: the registry diff that decides success cannot tell two apart.
	FMcpFabImportStatus Running;
	if (McpFabImportOperations::FindRunning(CacheLocation, Running))
	{
		FMcpFabAddResult Busy;
		if (Running.ListingId == ListingId && Running.Result.bAccepted)
		{
			// The caller repeated the add it already made, most likely after a timeout: hand back
			// the operation that is running rather than refuse or start a second one.
			Busy = Running.Result;
			Busy.bAlreadyRunning = true;
		}
		else
		{
			Busy.ErrorCode = TEXT("ALREADY_IN_FLIGHT");
			Busy.Error = DescribeRunning(Running);
		}
		Busy.OperationId = Running.OperationId;
		OnAccepted(Busy);
		return true;
	}

	const FString OperationId = McpFabImportOperations::Begin(ListingId);
	TSet<FString> Before = SnapshotGameAssets();

	FString Error;
	FString ErrorCode;
	const bool bDispatched = McpFabBridgeDispatch::Dispatch(
		[&ListingId, &EngineVersion, &Options](const FString& RequestId)
		{
			return BuildAddScript(RequestId, ListingId, EngineVersion, Options.CombineMeshes);
		},
		[Before = MoveTemp(Before), OperationId, OnAccepted, Options](bool bSuccess, const FString& Payload) mutable
		{
			FMcpFabAddResult Result = ParseAddReply(bSuccess, Payload);
			Result.OperationId = OperationId;
			if (!Result.bAccepted)
			{
				McpFabImportOperations::Finish(OperationId, Result);
				OnAccepted(Result);
				return;
			}
			// Only a listing Fab merges meshes for has anything to separate; the watcher works at it.
			if (Options.CombineMeshes.IsSet() && !Options.CombineMeshes.GetValue() && Result.bMergesMeshes)
			{
				Result.MeshesSeparated = false;
			}
			// Accepted only means the URL was handed over. Unreal decides success, later, and the
			// watcher records it under the operation id.
			McpFabImportOperations::Accept(OperationId, Result);
			McpFabImportWatcher::WatchForImport(OperationId, MoveTemp(Before), Result);
			OnAccepted(Result);
		},
		Error, ErrorCode);

	if (!bDispatched)
	{
		FMcpFabAddResult Failed;
		Failed.ErrorCode = ErrorCode;
		Failed.Error = Error;
		Failed.OperationId = OperationId;
		McpFabImportOperations::Finish(OperationId, Failed);
		OnAccepted(Failed);
	}
	return true;
}
} // namespace McpFabAddOperation

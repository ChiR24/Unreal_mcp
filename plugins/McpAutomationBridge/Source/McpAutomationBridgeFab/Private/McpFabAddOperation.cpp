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
#include "McpFabAddReply.h"
#include "McpFabAddScript.h"
#include "McpFabBridgeDispatch.h"
#include "Import/McpFabImportOperations.h"
#include "Import/McpFabImportWatcher.h"
#include "Import/McpFabInterchange.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Runtime/Launch/Resources/Version.h"
#include "AssetRegistry/IAssetRegistry.h"

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

/** How many adds may wait behind the running import. */
constexpr int32 MaxQueued = 8;


/**
 * Resolves the listing on Fab's page and, once Fab accepts, starts watching the import. Runs at once for
 * an add made while nothing else is going, or later for a queued one, when its turn comes. OnResolved is
 * the caller's reply while there still is a caller: a queued add was answered when it was queued and passes
 * none, so how it went is read from the status.
 */
void Launch(const FString& OperationId, const FString& ListingId, const FString& EngineVersion,
	const FMcpFabAddOptions& Options, TFunction<void(const FMcpFabAddResult&)> OnResolved)
{
	TSet<FString> Before = SnapshotGameAssets();

	FString Error;
	FString ErrorCode;
	const bool bDispatched = McpFabBridgeDispatch::Dispatch(
		[&ListingId, &EngineVersion, &Options](const FString& RequestId)
		{
			return BuildAddScript(RequestId, ListingId, EngineVersion, Options.CombineMeshes, Options.Quality);
		},
		[Before = MoveTemp(Before), OperationId, OnResolved, Options](bool bSuccess, const FString& Payload) mutable
		{
			FMcpFabAddResult Result = ParseAddReply(bSuccess, Payload);
			Result.OperationId = OperationId;
			if (!Result.bAccepted)
			{
				McpFabImportOperations::Finish(OperationId, Result);
				if (OnResolved) { OnResolved(Result); }
				return;
			}
			Result.Phase = TEXT("downloading");
			// Only a listing Fab merges meshes for has anything to separate; the watcher works at it.
			if (Options.CombineMeshes.IsSet() && !Options.CombineMeshes.GetValue() && Result.bMergesMeshes)
			{
				Result.MeshesSeparated = false;
			}
			// Accepted only means the URL was handed over. Unreal decides success, later, and the
			// watcher records it under the operation id.
			McpFabImportOperations::Accept(OperationId, Result);
			McpFabImportWatcher::WatchForImport(OperationId, MoveTemp(Before), Result, Options.PostImport, Options.SaveAgain);
			if (OnResolved) { OnResolved(Result); }
		},
		Error, ErrorCode);

	if (!bDispatched)
	{
		FMcpFabAddResult Failed;
		Failed.ErrorCode = ErrorCode;
		Failed.Error = Error;
		Failed.OperationId = OperationId;
		McpFabImportOperations::Finish(OperationId, Failed);
		if (OnResolved) { OnResolved(Failed); }
	}
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

	// The tier goes into the page script as text, so only a known one may.
	if (!IsKnownQuality(Options.Quality))
	{
		FMcpFabAddResult Unknown;
		Unknown.ErrorCode = TEXT("INVALID_QUALITY");
		Unknown.Error = TEXT("quality is one of raw, high, mid or low.");
		OnAccepted(Unknown);
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

	// The caller repeated an add that is already queued or running, most likely after a timeout: hand
	// back that operation rather than start a second one for the same listing.
	const FString OptionsKey = FString::Printf(TEXT("%s|%s|%s"), *Options.Quality,
		Options.CombineMeshes.IsSet() ? (Options.CombineMeshes.GetValue() ? TEXT("1") : TEXT("0")) : TEXT(""), *Options.RequestKey);
	FMcpFabImportStatus Open;
	if (McpFabImportOperations::FindOpenByListing(ListingId, CacheLocation, Open))
	{
		if (McpFabImportOperations::OptionsKeyOf(Open.OperationId) != OptionsKey)
		{
			FMcpFabAddResult Changed;
			Changed.ErrorCode = TEXT("ADD_ALREADY_RUNNING");
			Changed.Error = FString::Printf(TEXT("%s is already being added (%s) with different quality, combineMeshes, destinationPath "
				"or assetName; it would ignore these. Wait for it, or cancel it and add again."), *ListingId, *Open.OperationId);
			Changed.OperationId = Open.OperationId;
			OnAccepted(Changed);
			return true;
		}
		FMcpFabAddResult Same = Open.Result;
		Same.bAccepted = true;
		Same.bAlreadyRunning = true;
		Same.OperationId = Open.OperationId;
		Same.Phase = Open.Phase;
		Same.QueuePosition = Open.QueuePosition;
		OnAccepted(Same);
		return true;
	}

	// One import runs at a time: the registry diff that decides success cannot tell two apart. An add
	// made behind a running or queued one is queued and starts by itself, oldest first.
	TArray<FMcpFabImportStatus> Queue;
	McpFabImportOperations::ListQueue(CacheLocation, Queue);
	const bool bBusy = Queue.Num() > 0;
	if (bBusy && McpFabImportOperations::QueuedCount() >= MaxQueued)
	{
		FMcpFabAddResult Full;
		Full.ErrorCode = TEXT("QUEUE_FULL");
		Full.Error = DescribeQueueHead(Queue[0], MaxQueued);
		Full.OperationId = Queue[0].OperationId;
		OnAccepted(Full);
		return true;
	}

	const FString OperationId = McpFabImportOperations::Begin(ListingId, OptionsKey);
	if (!bBusy)
	{
		Launch(OperationId, ListingId, EngineVersion, Options, OnAccepted);
		return true;
	}
	McpFabImportOperations::Enqueue(OperationId, [OperationId, ListingId, EngineVersion, Options]()
	{
		Launch(OperationId, ListingId, EngineVersion, Options, nullptr);
	});
	FMcpFabAddResult Queued;
	Queued.bAccepted = true;
	Queued.OperationId = OperationId;
	Queued.Phase = TEXT("queued");
	Queued.QueuePosition = McpFabImportOperations::QueuedCount();
	OnAccepted(Queued);
	return true;
}
} // namespace McpFabAddOperation

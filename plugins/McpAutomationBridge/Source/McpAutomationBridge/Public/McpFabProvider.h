// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "McpFabTypes.h"

/**
 * The seam that keeps Fab out of the core module.
 *
 * PrivateDependencyModuleNames produces real DLL imports, so a core module that
 * referenced FFabDownloadRequest imported Fab.dll whether or not the surrounding
 * `#if MCP_HAS_FAB` was taken. Calling the Unreal plugin "optional" did not make
 * those imports optional: on an engine where Fab was present at build time but
 * unmounted at runtime, the loader failed the whole plugin with
 * ERROR_MOD_NOT_FOUND and no MCP tool worked at all.
 *
 * Everything below is expressed in engine types only. The adapter module
 * McpAutomationBridgeFab implements it, hard-links Fab and MegascansPlugin, and
 * registers itself through the modular-features registry. Core asks the registry
 * and degrades to a NOT_SUPPORTED receipt when nobody answered, which is the
 * behaviour "optional Fab support" was always supposed to mean.
 */

class IMcpFabProvider : public IModularFeature
{
public:
	static FName FeatureName() { return FName(TEXT("McpFabProvider")); }

	virtual ~IMcpFabProvider() = default;

	/** False when the adapter was built against an engine with no Fab module. */
	virtual bool IsFabAvailable() const = 0;

	/** False when the adapter was built against an engine with no Bridge plugin. */
	virtual bool IsMegascansAvailable() const = 0;

	/** Fab's own cache location, which can differ from the ini until it flushes. */
	virtual FString GetCacheLocation() const = 0;

	virtual void GetCachedAssets(TArray<FMcpFabCachedAsset>& OutAssets) const = 0;

	/**
	 * Queues a transfer on Fab's downloader. OnComplete runs on whatever thread
	 * the queue completes on; callers marshal as needed. Returns false when Fab
	 * is unavailable, in which case OnComplete never runs.
	 */
	virtual bool EnqueueDownload(
		const FString& AssetId,
		const FString& Url,
		const FString& DestinationDirectory,
		bool bUseBuildPatch,
		TFunction<void(const FMcpFabDownloadResult&)> OnComplete) = 0;

	/** Hands a Bridge export envelope to the Megascans importer. */
	virtual bool ImportMegascansEnvelope(const FString& SerializedJson, FString& OutError) = 0;

	/**
	 * Starts importing one Fab listing through the signed-in Fab page and reports as soon as Fab has
	 * accepted the download, or refused it.
	 *
	 * The download and the import take minutes and hold the game thread, far past any client's request
	 * timeout, so OnAccepted fires once, on the game thread, with OperationId set; the import then
	 * continues in the background and GetImportStatus reports how it is going. One import runs at a
	 * time: an add made while another runs is queued (Phase queued) and starts by itself, in order. Returns false when the
	 * adapter cannot even start (Fab absent), in which case OnAccepted never runs.
	 */
	virtual bool AddToProject(
		const FString& ListingId,
		const FMcpFabAddOptions& Options,
		TFunction<void(const FMcpFabAddResult&)> OnAccepted) = 0;

	/**
	 * Reports one background import, found by its operation id or by the listing it imports (the
	 * newest operation for that listing). Returns false when no such operation is known; the store
	 * holds the last few operations of this editor session.
	 */
	virtual bool GetImportStatus(const FString& OperationOrListingId, FMcpFabImportStatus& OutStatus) = 0;

	/** The import that is running, then every add waiting behind it, in the order they will start. */
	virtual void GetImportQueue(TArray<FMcpFabImportStatus>& OutQueue) = 0;

	/**
	 * Cancels one import where Fab gives a way to: an add still waiting in the queue is dropped, a
	 * unreal-engine pack download is stopped by pressing Fab's own Cancel button, and an import Interchange
	 * is translating is told to cancel its tasks. A download of a source format has no cancel and is
	 * refused. Returns false with OutErrorCode and OutMessage saying why; on success OutMessage says what was done.
	 */
	virtual bool CancelImport(const FString& OperationId, FString& OutMessage, FString& OutErrorCode) = 0;

	/**
	 * Fetches one listing's description, facts and preview image, and what adding it would do.
	 *
	 * OutJson carries imageBase64 rather than a URL, which McpJsonRpcImageContent
	 * promotes into a real MCP image block, so the caller sees the asset.
	 */
	virtual bool GetListingDetails(
		const FString& ListingId,
		TFunction<void(bool /*bSuccess*/, const FString& /*Json*/)> OnComplete) = 0;

	/**
	 * Queries the Fab catalog through the signed-in page.
	 *
	 * No channel filter is applied -- pinning one hid the Quixel/Megascans library -- so a hit is a
	 * candidate, not a promise. The request may narrow by publisher and by content kind; every row
	 * carries the publisher, category, rating, price and formats the search itself returned.
	 */
	virtual bool SearchListings(
		const FMcpFabSearchRequest& Request,
		TFunction<void(const FMcpFabSearchResult&)> OnComplete) = 0;
};

/** Null whenever the adapter module is absent — the entire point of the split. */
MCPAUTOMATIONBRIDGE_API IMcpFabProvider* GetMcpFabProvider();

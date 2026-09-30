// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

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

/** One completed transfer, in terms core understands. */
struct FMcpFabDownloadResult
{
	bool bSuccess = false;
	bool bServedFromCache = false;
	uint64 CompletedBytes = 0;
	uint64 TotalBytes = 0;
	TArray<FString> DownloadedFiles;
	FString Error;
};

/**
 * What one add-to-project request learned from Fab's page, and later how the import ended.
 *
 * Deliberately carries no URL and no credential: the signed URL is minted and
 * consumed inside Fab's page, so nothing here can leak it into a receipt.
 * AssetPaths is the honest evidence of success -- Fab accepting the workflow is
 * not the same as content existing, and only the asset registry settles that.
 */
struct FMcpFabAddResult
{
	bool bAccepted = false;
	bool bTimedOut = false;
	FString Error;
	FString ErrorCode;
	FString RootPath;
	int32 AssetCount = 0;
	TArray<FString> SamplePaths;
	bool bEngineExactMatch = false;
	FString VersionName;
	/** The background import this add started; for ALREADY_IN_FLIGHT, the one still running. */
	FString OperationId;
	/** True when the same listing was already being imported and this add joined that operation. */
	bool bAlreadyRunning = false;
	/** The format Fab was asked to import: unreal-engine, gltf, glb, fbx, obj or usdz. */
	FString FormatCode;
	/** Quality tier of the chosen file (raw, high, mid, low); empty when the listing has none. */
	FString Quality;
	/** The file Fab downloads. */
	FString FileName;
	/** Bytes of that file, or -1 when Fab publishes no size for it (unreal-engine packs). */
	int64 DownloadBytes = -1;
};

/** Where one background import stands. Read-only: assembled from the operation store on demand. */
struct FMcpFabImportStatus
{
	FString OperationId;
	FString ListingId;
	/** resolving, downloading, importing, done or failed. */
	FString Phase;
	/** Seconds since the add was requested; frozen once the import finished. */
	double ElapsedSeconds = 0.0;
	/** Bytes fetched so far, or -1 when the download cannot be observed. */
	int64 DownloadedBytes = -1;
	/** New assets seen in the registry so far. */
	int32 AssetsSoFar = 0;
	/** What the add reported, completed with the outcome once the import is done or failed. */
	FMcpFabAddResult Result;
	/** Error lines Fab logged while the import ran, scrubbed. */
	TArray<FString> FabErrors;
};

/** One catalog hit. Carries an id and labels only -- never a URL. */
struct FMcpFabListing
{
	FString Uid;
	FString Title;
	FString ListingType;
	/** Derived from price, not from the listing's own isFree flag. */
	bool bIsFree = false;
	/** False when the price field could not be interpreted; see PriceShape. */
	bool bPriceResolved = false;
	/** Key names of an unrecognised price object, so the next run can be fixed. */
	FString PriceShape;
	/** The listing's raw isFree flag, kept because it disagrees with price. */
	bool bRawIsFree = false;
	TArray<FString> Tags;
};

/** Outcome of a catalog query. */
struct FMcpFabSearchResult
{
	bool bSuccess = false;
	FString Error;
	FString ErrorCode;
	TArray<FMcpFabListing> Listings;
};

/** One pack the Fab plugin has already pulled to this machine. */
struct FMcpFabCachedAsset
{
	FString AssetId;
	FString CachedFile;
};

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
	 * continues in the background and GetImportStatus reports how it is going. Returns false when the
	 * adapter cannot even start (Fab absent), in which case OnAccepted never runs.
	 */
	virtual bool AddToProject(
		const FString& ListingId,
		TFunction<void(const FMcpFabAddResult&)> OnAccepted) = 0;

	/**
	 * Reports one background import, found by its operation id or by the listing it imports (the
	 * newest operation for that listing). Returns false when no such operation is known; the store
	 * holds the last few operations of this editor session.
	 */
	virtual bool GetImportStatus(const FString& OperationOrListingId, FMcpFabImportStatus& OutStatus) = 0;

	/**
	 * Queries the Fab catalog through the signed-in page.
	 *
	 * The channel filter is fixed natively to unreal-engine so every hit is a
	 * listing AddToProject can actually consume; a caller supplies only the free
	 * text and paging.
	 */
	/**
	 * Fetches one listing's description and preview image.
	 *
	 * OutJson carries imageBase64 rather than a URL, which McpJsonRpcImageContent
	 * promotes into a real MCP image block, so the caller sees the asset.
	 */
	virtual bool GetListingDetails(
		const FString& ListingId,
		TFunction<void(bool /*bSuccess*/, const FString& /*Json*/)> OnComplete) = 0;

	virtual bool SearchListings(
		const FString& Query,
		bool bFreeOnly,
		int32 Limit,
		TFunction<void(const FMcpFabSearchResult&)> OnComplete) = 0;
};

/** Null whenever the adapter module is absent — the entire point of the split. */
MCPAUTOMATIONBRIDGE_API IMcpFabProvider* GetMcpFabProvider();

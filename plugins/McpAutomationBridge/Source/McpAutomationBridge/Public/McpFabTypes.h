// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

// The plain data the Fab provider seam passes between the core module and the adapter. Engine types only:
// no Fab or Megascans type may appear here, so core can include this without linking either.

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
	/** Which build of a unreal-engine pack was taken: exact, older or newer than the running engine; empty when unknown. */
	FString EngineMatch;
	/** The engine version that build declares (UE_5.4), empty when none was declared. */
	FString EngineVersion;
	FString VersionName;
	/** The background import this add started; for QUEUE_FULL, the one still running. */
	FString OperationId;
	/** True when the same listing was already queued or being imported and this add joined that operation. */
	bool bAlreadyRunning = false;
	/** Where the operation stood when this replied: queued, resolving or downloading. */
	FString Phase;
	/** 1-based place in the queue while queued, else 0. */
	int32 QueuePosition = 0;
	/** The listing's title, as Fab names it; empty until the page has been asked. */
	FString Title;
	/** The format Fab was asked to import: unreal-engine, gltf, glb, fbx, obj or usdz. */
	FString FormatCode;
	/** Quality tier of the chosen file (raw, high, mid, low); empty when the listing has none. */
	FString Quality;
	/** The file Fab downloads. */
	FString FileName;
	/** Bytes of that file, or -1 when Fab publishes no size for it (unreal-engine packs). */
	int64 DownloadBytes = -1;
	/** True when Fab's importer for this listing merges every mesh of a file into ONE static mesh. */
	bool bMergesMeshes = false;
	/** Set only when the add asked for separate meshes: whether Interchange's combining was switched off in time. */
	TOptional<bool> MeshesSeparated;
	/** True once the post-import step ran: it saves the packages the import left dirty. */
	bool bSaveRan = false;
	/** Packages that step saved. One that arrived on disk already (a unreal-engine pack) is not dirty and not counted. */
	int32 SavedCount = 0;
	/** Packages that step could not save, by name. */
	TArray<FString> UnsavedPackages;
};

/** What the caller asked the add to do beyond importing the listing. */
struct FMcpFabAddOptions
{
	/**
	 * Unset leaves Fab's own behaviour, which merges every mesh in a source file into ONE static mesh.
	 * false imports each mesh as its own asset; true accepts the single merged mesh explicitly, which a
	 * scene-sized mesh file requires before it is downloaded at all.
	 */
	TOptional<bool> CombineMeshes;

	/**
	 * Which quality tier of a source format to fetch: raw, high, mid or low. Empty takes the best
	 * game-ready tier (high, else mid, low, raw). A listing that publishes no tiers, and a unreal-engine
	 * pack, ignore it; a tier the listing lacks is refused with QUALITY_NOT_AVAILABLE.
	 */
	FString Quality;

	/**
	 * Runs once, on the game thread, when the import has settled and before its outcome is stored, with
	 * every asset path the import created. The adapter module cannot save or move assets itself -- the
	 * safe wrappers live in the core -- so the core does it here and reports what it did by editing Result.
	 */
	TFunction<void(FMcpFabAddResult& Result, const TArray<FString>& ImportedPaths)> PostImport;
};

/** Where one background import stands. Read-only: assembled from the operation store on demand. */
struct FMcpFabImportStatus
{
	FString OperationId;
	FString ListingId;
	/** queued, resolving, downloading, importing, cancelling, done or failed. */
	FString Phase;
	/** 1-based place in the queue while queued, else 0. */
	int32 QueuePosition = 0;
	/** Seconds since the add was requested; frozen once the import finished. */
	double ElapsedSeconds = 0.0;
	/** Bytes fetched so far, or -1 when the download cannot be observed. */
	int64 DownloadedBytes = -1;
	/** The percent Fab's own "Downloading ..." notification shows (0-100), or -1 when there is none. */
	float DownloadPercent = -1.0f;
	/** True while CancelImport would stop this import right now. */
	bool bCancellable = false;
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
	/** Derived from price: the listing's own isFree flag disagrees with it and is not carried. */
	bool bIsFree = false;
	/** False when the price field could not be interpreted; see PriceShape. */
	bool bPriceResolved = false;
	/** Key names of an unrecognised price object, so the next run can be fixed. */
	FString PriceShape;
	/** The publisher's name; empty when the row carries none. */
	FString Seller;
	FString Category;
	/** The facts below are set only when the row carried them: unset is unknown, not zero. */
	TOptional<double> AverageRating;
	TOptional<int32> RatingCount;
	TOptional<double> Price;
	FString Currency;
	TOptional<bool> bIsCc0;
	FString PublishedAt;
	/** Format codes the listing ships (unreal-engine, gltf, fbx, ...). */
	TArray<FString> Formats;
	TArray<FString> Tags;
};

/** What a catalog query asks for. */
struct FMcpFabSearchRequest
{
	/** Free text; empty for none. */
	FString Query;
	/** One publisher's name, such as Quixel Megascans; empty for every publisher. */
	FString Seller;
	/** One content kind, such as 3d-model or material; empty for every kind. */
	FString ListingType;
	bool bFreeOnly = false;
	int32 Limit = 12;
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

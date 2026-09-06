// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabAddScript.h"
#include "McpFabProvider.h"

#include "Features/IModularFeatures.h"

namespace McpFabDetailsOperation
{
bool Start(const FString& ListingId, TFunction<void(bool, const FString&)> OnComplete);
}

namespace McpFabSearchOperation
{
bool Start(const FString& Query, bool bFreeOnly, int32 Limit,
	TFunction<void(const FMcpFabSearchResult&)> OnComplete);
}

namespace McpFabAddOperation
{
bool Start(const FString& ListingId, const FString& EngineVersion,
	TFunction<void(const FMcpFabAddResult&)> OnComplete);
}
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#if MCP_FAB_ADAPTER_HAS_FAB
#include "FabDownloader.h"
#include "Utilities/FabAssetsCache.h"
#endif
#if MCP_FAB_ADAPTER_HAS_MEGASCANS
#include "AssetsImportController.h"
#endif
#endif

namespace
{
/**
 * Concrete adapter. Every Fab and Megascans symbol in the plugin lives here.
 *
 * The `#if` blocks decide what this build can do; the interface itself carries
 * no Fab types, so core links against the declaration alone and keeps working
 * when this module is missing entirely.
 */
class FMcpFabProvider final : public IMcpFabProvider
{
public:
	// Compiled-in support is necessary but not sufficient: the imports are
	// delay-loaded, so calling into an unmounted Fab would fault on the stub
	// rather than fail cleanly. IsModuleLoaded is the check that makes
	// "available: false" an answer instead of a crash.
	virtual bool IsFabAvailable() const override
	{
#if WITH_EDITOR && MCP_FAB_ADAPTER_HAS_FAB
		return FModuleManager::Get().IsModuleLoaded(TEXT("Fab"));
#else
		return false;
#endif
	}

	virtual bool IsMegascansAvailable() const override
	{
#if WITH_EDITOR && MCP_FAB_ADAPTER_HAS_MEGASCANS
		return FModuleManager::Get().IsModuleLoaded(TEXT("MegascansPlugin"));
#else
		return false;
#endif
	}

	virtual FString GetCacheLocation() const override
	{
#if WITH_EDITOR && MCP_FAB_ADAPTER_HAS_FAB
		return FFabAssetsCache::GetCacheLocation();
#else
		return FString();
#endif
	}

	virtual void GetCachedAssets(TArray<FMcpFabCachedAsset>& OutAssets) const override
	{
#if WITH_EDITOR && MCP_FAB_ADAPTER_HAS_FAB
		for (const FString& AssetId : FFabAssetsCache::GetCachedAssets())
		{
			FMcpFabCachedAsset& Entry = OutAssets.AddDefaulted_GetRef();
			Entry.AssetId = AssetId;
			Entry.CachedFile = FFabAssetsCache::GetCachedFile(AssetId);
		}
#endif
	}

	virtual bool EnqueueDownload(
		const FString& AssetId,
		const FString& Url,
		const FString& DestinationDirectory,
		bool bUseBuildPatch,
		TFunction<void(const FMcpFabDownloadResult&)> OnComplete) override
	{
#if WITH_EDITOR && MCP_FAB_ADAPTER_HAS_FAB
		const EFabDownloadType Type =
			bUseBuildPatch ? EFabDownloadType::BuildPatchRequest : EFabDownloadType::HTTP;

		// Owned by the download queue for the lifetime of the transfer; the queue
		// drives ExecuteRequest and retains it until completion.
		FFabDownloadRequest* Request =
			new FFabDownloadRequest(AssetId, Url, DestinationDirectory, Type);

		Request->OnDownloadComplete().AddLambda(
			[OnComplete](const FFabDownloadRequest*, const FFabDownloadStats& Stats)
			{
				FMcpFabDownloadResult Result;
				Result.bSuccess = Stats.bIsSuccess;
				Result.bServedFromCache = Stats.bIsCached;
				Result.CompletedBytes = Stats.CompletedBytes;
				Result.TotalBytes = Stats.TotalBytes;
				Result.DownloadedFiles = Stats.DownloadedFiles;
				OnComplete(Result);
			});

		FFabDownloadQueue::AddDownloadToQueue(Request);
		return true;
#else
		return false;
#endif
	}

	virtual bool AddToProject(
		const FString& ListingId,
		TFunction<void(const FMcpFabAddResult&)> OnComplete) override
	{
#if WITH_EDITOR && MCP_FAB_ADAPTER_HAS_FAB
		if (!McpFabAddOperation::IsSafeListingIdShared(ListingId))
		{
			FMcpFabAddResult Rejected;
			Rejected.ErrorCode = TEXT("INVALID_LISTING_ID");
			Rejected.Error = TEXT("A listing id must be [A-Za-z0-9_-] and at most 64 characters.");
			OnComplete(Rejected);
			return true;
		}
		return McpFabAddOperation::Start(
			ListingId, McpFabAddOperation::CurrentEngineVersion(), MoveTemp(OnComplete));
#else
		return false;
#endif
	}

	virtual bool GetListingDetails(
		const FString& ListingId,
		TFunction<void(bool, const FString&)> OnComplete) override
	{
#if WITH_EDITOR && MCP_FAB_ADAPTER_HAS_FAB
		return McpFabDetailsOperation::Start(ListingId, MoveTemp(OnComplete));
#else
		return false;
#endif
	}

	virtual bool SearchListings(
		const FString& Query,
		bool bFreeOnly,
		int32 Limit,
		TFunction<void(const FMcpFabSearchResult&)> OnComplete) override
	{
#if WITH_EDITOR && MCP_FAB_ADAPTER_HAS_FAB
		return McpFabSearchOperation::Start(Query, bFreeOnly, Limit, MoveTemp(OnComplete));
#else
		return false;
#endif
	}

	virtual bool ImportMegascansEnvelope(const FString& SerializedJson, FString& OutError) override
	{
#if WITH_EDITOR && MCP_FAB_ADAPTER_HAS_MEGASCANS
		TSharedPtr<FAssetsImportController> Controller = FAssetsImportController::Get();
		if (!Controller.IsValid())
		{
			OutError = TEXT("Megascans import controller unavailable.");
			return false;
		}
		Controller->DataReceived(SerializedJson);
		return true;
#else
		OutError = TEXT("This build has no MegascansPlugin module.");
		return false;
#endif
	}
};

FMcpFabProvider GProvider;
} // namespace

class FMcpAutomationBridgeFabModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(IMcpFabProvider::FeatureName(), &GProvider);
	}

	virtual void ShutdownModule() override
	{
		IModularFeatures::Get().UnregisterModularFeature(IMcpFabProvider::FeatureName(), &GProvider);
	}
};

IMPLEMENT_MODULE(FMcpAutomationBridgeFabModule, McpAutomationBridgeFab)

// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabAddScript.h"
#include "McpFabProvider.h"
#include "Import/McpFabImportOperations.h"
#include "Import/McpFabLogCapture.h"

#include "Features/IModularFeatures.h"

namespace McpFabDetailsOperation
{
bool Start(const FString& ListingId, const FString& EngineVersion, TFunction<void(bool, const FString&)> OnComplete);
}

namespace McpFabSearchOperation
{
bool Start(const FMcpFabSearchRequest& Request, TFunction<void(const FMcpFabSearchResult&)> OnComplete);
}

namespace McpFabAddOperation
{
bool Start(const FString& ListingId, const FString& EngineVersion, const FString& CacheLocation,
	const FMcpFabAddOptions& Options, TFunction<void(const FMcpFabAddResult&)> OnAccepted);
}
#include "Misc/EngineVersion.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Modules/ModuleManager.h"

// FabDownloader.h / FabAssetsCache.h are Private (not includable) until UE 5.7.
#if MCP_FAB_ADAPTER_HAS_FAB && __has_include("FabDownloader.h") && __has_include("Utilities/FabAssetsCache.h")
#define MCP_FAB_ADAPTER_HAS_FAB_API 1
#include "FabDownloader.h"
#include "Utilities/FabAssetsCache.h"
#else
#define MCP_FAB_ADAPTER_HAS_FAB_API 0
#endif
#if MCP_FAB_ADAPTER_HAS_MEGASCANS
#include "AssetsImportController.h"
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
#if MCP_FAB_ADAPTER_HAS_FAB_API
		return FModuleManager::Get().IsModuleLoaded(TEXT("Fab"));
#else
		return false;
#endif
	}

	virtual bool IsMegascansAvailable() const override
	{
#if MCP_FAB_ADAPTER_HAS_MEGASCANS
		return FModuleManager::Get().IsModuleLoaded(TEXT("MegascansPlugin"));
#else
		return false;
#endif
	}

	virtual FString GetCacheLocation() const override
	{
#if MCP_FAB_ADAPTER_HAS_FAB_API
		return FFabAssetsCache::GetCacheLocation();
#else
		return FString();
#endif
	}

	virtual void GetCachedAssets(TArray<FMcpFabCachedAsset>& OutAssets) const override
	{
#if MCP_FAB_ADAPTER_HAS_FAB_API
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
#if MCP_FAB_ADAPTER_HAS_FAB_API
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
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
				Result.bServedFromCache = Stats.bIsCached;
#endif
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
		const FMcpFabAddOptions& Options,
		TFunction<void(const FMcpFabAddResult&)> OnAccepted) override
	{
#if MCP_FAB_ADAPTER_HAS_FAB_API
		const FEngineVersion& Version = FEngineVersion::Current();
		return McpFabAddOperation::Start(ListingId,
			FString::Printf(TEXT("%u.%u"), Version.GetMajor(), Version.GetMinor()), GetCacheLocation(),
			Options, MoveTemp(OnAccepted));
#else
		return false;
#endif
	}

	virtual bool GetImportStatus(const FString& OperationOrListingId, FMcpFabImportStatus& OutStatus) override
	{
		return McpFabImportOperations::Find(OperationOrListingId, GetCacheLocation(), OutStatus);
	}

	virtual void GetImportQueue(TArray<FMcpFabImportStatus>& OutQueue) override
	{
		McpFabImportOperations::ListQueue(GetCacheLocation(), OutQueue);
	}

	virtual bool CancelImport(const FString& OperationId, FString& OutMessage, FString& OutErrorCode) override
	{
		return McpFabImportOperations::RequestCancel(OperationId, OutMessage, OutErrorCode);
	}

	virtual bool GetListingDetails(
		const FString& ListingId,
		TFunction<void(bool, const FString&)> OnComplete) override
	{
#if MCP_FAB_ADAPTER_HAS_FAB_API
		const FEngineVersion& Version = FEngineVersion::Current();
		return McpFabDetailsOperation::Start(ListingId,
			FString::Printf(TEXT("%u.%u"), Version.GetMajor(), Version.GetMinor()), MoveTemp(OnComplete));
#else
		return false;
#endif
	}

	virtual bool SearchListings(
		const FMcpFabSearchRequest& Request,
		TFunction<void(const FMcpFabSearchResult&)> OnComplete) override
	{
#if MCP_FAB_ADAPTER_HAS_FAB_API
		return McpFabSearchOperation::Start(Request, MoveTemp(OnComplete));
#else
		return false;
#endif
	}

	virtual bool ImportMegascansEnvelope(const FString& SerializedJson, FString& OutError) override
	{
#if MCP_FAB_ADAPTER_HAS_MEGASCANS
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
		McpFabLogCapture::Stop();
		IModularFeatures::Get().UnregisterModularFeature(IMcpFabProvider::FeatureName(), &GProvider);
	}
};

IMPLEMENT_MODULE(FMcpAutomationBridgeFabModule, McpAutomationBridgeFab)

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

#include "McpFabProvider.h"
#include "McpFabAddScript.h"
#include "McpFabBridgeDispatch.h"
#include "McpFabImportWatcher.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpFabAddOp, Log, All);

namespace McpFabAddOperation
{
FString BuildAddScript(const FString& RequestId, const FString& ListingId, const FString& EngineVersion);

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
		Paths.Add(Asset.GetObjectPathString());
	}
	return Paths;
}
} // namespace

/** Shared entry point used by the provider implementation. */
bool Start(const FString& ListingId, const FString& EngineVersion,
	TFunction<void(const FMcpFabAddResult&)> OnComplete)
{
	// Fail closed at this entry point too: the provider validates, but the
	// function interpolates ListingId into a JS string literal inside Fab's
	// authenticated page, so the allowlist belongs here as well -- an id that
	// cannot reach the page cannot steer the path.
	if (!IsSafeListingIdShared(ListingId))
	{
		FMcpFabAddResult Rejected;
		Rejected.ErrorCode = TEXT("INVALID_ARGUMENT");
		Rejected.Error = TEXT("ListingId must be a Fab listing uid of [A-Za-z0-9_-], 64 chars max.");
		OnComplete(Rejected);
		return true;
	}

	if (McpFabImportWatcher::IsBusy())
	{
		FMcpFabAddResult Busy;
		Busy.ErrorCode = TEXT("ALREADY_IN_FLIGHT");
		Busy.Error = TEXT("Another Fab add is still running.");
		OnComplete(Busy);
		return true;
	}

	TSet<FString> Before = SnapshotGameAssets();

	FString Error;
	FString ErrorCode;
	const bool bDispatched = McpFabBridgeDispatch::Dispatch(
		[&ListingId, &EngineVersion](const FString& RequestId)
		{
			return BuildAddScript(RequestId, ListingId, EngineVersion);
		},
		[Before = MoveTemp(Before), OnComplete](bool bSuccess, const FString& Payload) mutable
		{
			FMcpFabAddResult Result;
			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Payload);
			// Parsed even on failure: the page reports why it refused, and
			// gating the parse on bSuccess turned every such answer into a bare
			// FAB_REJECTED that named nothing.
			const bool bJson = FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid();
			if (bJson) { Root->TryGetStringField(TEXT("error"), Result.ErrorCode); }
			const bool bParsed = bSuccess && bJson;
			if (bParsed)
			{
				Root->TryGetBoolField(TEXT("accepted"), Result.bAccepted);
				Root->TryGetBoolField(TEXT("engineExactMatch"), Result.bEngineExactMatch);
				Root->TryGetStringField(TEXT("versionName"), Result.VersionName);
				Root->TryGetStringField(TEXT("error"), Result.ErrorCode);
			}
			if (!Result.bAccepted)
			{
				McpFabImportWatcher::SetBusy(false);
				// Safe to log in full: the page reports codes, statuses and key
				// shapes, never values -- shape() emits key names and types, and
				// the download url stays a local in the script. Without this the
				// self-diagnosing fields the script already computes never reach
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
				OnComplete(Result);
				return;
			}
			// Accepted only means the URL was handed over. Unreal decides success.
			McpFabImportWatcher::WatchForImport(MoveTemp(Before), Result, OnComplete);
		},
		Error, ErrorCode);

	if (!bDispatched)
	{
		FMcpFabAddResult Failed;
		Failed.ErrorCode = ErrorCode;
		Failed.Error = Error;
		OnComplete(Failed);
		return true;
	}
	McpFabImportWatcher::SetBusy(true);
	return true;
}
} // namespace McpFabAddOperation

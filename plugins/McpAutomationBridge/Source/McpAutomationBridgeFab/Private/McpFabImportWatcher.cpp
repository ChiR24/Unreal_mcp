// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabImportWatcher.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/Ticker.h"
#include "Misc/ScopeLock.h"

namespace McpFabImportWatcher
{
namespace
{
/** A pack can be gigabytes; this is a ceiling, not an expectation. */
constexpr double MaxWaitSeconds = 600.0;
/** How long the registry must stay quiet before the import is called done. */
constexpr double SettleSeconds = 6.0;

/** Guards the whole add, not just the page call. */
bool bOperationInFlight = false;

/** Longest shared /Game/<folder> prefix of everything that appeared. */
FString CommonRoot(const TArray<FString>& Paths)
{
	FString Root;
	for (const FString& Path : Paths)
	{
		FString Remainder = Path;
		if (!Remainder.RemoveFromStart(TEXT("/Game/")))
		{
			continue;
		}
		FString Folder;
		if (!Remainder.Split(TEXT("/"), &Folder, nullptr))
		{
			continue;
		}
		const FString Candidate = TEXT("/Game/") + Folder;
		if (Root.IsEmpty()) { Root = Candidate; }
		else if (Root != Candidate) { return TEXT("/Game"); }
	}
	return Root.IsEmpty() ? TEXT("/Game") : Root;
}
} // namespace

bool IsBusy()
{
	return bOperationInFlight;
}

void SetBusy(bool bBusy)
{
	bOperationInFlight = bBusy;
}

void WatchForImport(
	TSet<FString> Before,
	FMcpFabAddResult Partial,
	TFunction<void(const FMcpFabAddResult&)> OnComplete)
{
	TSharedRef<double> Elapsed = MakeShared<double>(0.0);
	TSharedRef<double> QuietFor = MakeShared<double>(0.0);
	TSharedRef<int32> LastCount = MakeShared<int32>(0);
	TSharedRef<TSet<FString>> AddedSet = MakeShared<TSet<FString>>();
	TSharedRef<FCriticalSection> AddedLock = MakeShared<FCriticalSection>();
	TSharedRef<FDelegateHandle> AddedHandle = MakeShared<FDelegateHandle>();
	TSharedRef<FDelegateHandle> TickerHandle = MakeShared<FDelegateHandle>();

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
		TEXT("AssetRegistry")).Get();
	*AddedHandle = Registry.OnAssetAdded.AddLambda(
		[Before, AddedSet, AddedLock](const FAssetData& AssetData)
		{
			const FString Path = AssetData.GetObjectPathString();
			// Skip the baseline and sub-objects: a map contributes entries like
			// Map.Map:PersistentLevel.ActorFolder_UID_..., which are parts of
			// one asset rather than assets.
			if (Before.Contains(Path) || Path.Contains(TEXT(":")))
			{
				return;
			}
			FScopeLock Lock(AddedLock.Get());
			AddedSet->Add(Path);
		});

	*TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[Partial, OnComplete, Elapsed, QuietFor, LastCount, AddedSet, AddedLock, AddedHandle, TickerHandle](float Delta) mutable
		{
			*Elapsed += Delta;

			int32 Count;
			{
				FScopeLock Lock(AddedLock.Get());
				Count = AddedSet->Num();
			}
			if (Count != *LastCount) { *LastCount = Count; *QuietFor = 0.0; }
			else if (Count > 0) { *QuietFor += Delta; }

			const bool bSettled = Count > 0 && *QuietFor >= SettleSeconds;
			const bool bExpired = *Elapsed >= MaxWaitSeconds;
			if (!bSettled && !bExpired)
			{
				return true; // keep ticking
			}

			TArray<FString> Added;
			{
				FScopeLock Lock(AddedLock.Get());
				Added = AddedSet->Array();
			}
			Added.Sort();
			Partial.AssetCount = Count;
			Partial.RootPath = CommonRoot(Added);
			Partial.SamplePaths.Reset();
			for (int32 Index = 0; Index < Count && Index < 10; ++Index)
			{
				Partial.SamplePaths.Add(Added[Index]);
			}
			if (Count == 0)
			{
				Partial.bTimedOut = true;
				Partial.ErrorCode = TEXT("IMPORT_TIMED_OUT");
				Partial.Error = FString::Printf(
					TEXT("Fab accepted the workflow but no new asset appeared within %.0f seconds."),
					MaxWaitSeconds);
			}
			else if (bExpired)
			{
				// The ceiling was reached while packages were still streaming:
				// report what landed but mark it, so the caller does not mistake
				// a partial import for a settled one.
				Partial.bTimedOut = true;
				Partial.ErrorCode = TEXT("IMPORT_PARTIAL");
				Partial.Error = FString::Printf(
					TEXT("The import was still streaming when the %.0f-second ceiling was reached; %d asset(s) landed."),
					MaxWaitSeconds, Count);
			}

			IAssetRegistry& RegistryRef = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
				TEXT("AssetRegistry")).Get();
			RegistryRef.OnAssetAdded.Remove(*AddedHandle);
			FTSTicker::GetCoreTicker().RemoveTicker(*TickerHandle);
			bOperationInFlight = false;
			OnComplete(Partial);
			return false;
		}), 1.0f);
}
} // namespace McpFabImportWatcher

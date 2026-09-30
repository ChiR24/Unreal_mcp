// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabImportWatcher.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Runtime/Launch/Resources/Version.h"
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

// Fab's import raises modal dialogs mid-flight -- FGenericImportWorkflow asks
// "Do you want to open the file to manually Extract and Import?" whenever it
// meets an archive it cannot unpack, such as a .rar nested inside the .zip.
// A modal in an editor nobody is sitting at blocks the game thread forever:
// the bridge stops answering, and the caller times out with no clue that a
// dialog is the reason. One such prompt froze an entire session here.
//
// Every MCP request already runs under GIsRunningUnattendedScript, which
// FMessageDialog honours by returning a default instead of showing UI. But
// that guard is scoped to the REQUEST, and Fab's workflow is asynchronous --
// it unwinds long before the dialog appears. So the flag is held for the
// import's own lifetime instead, released by RAII when the watcher's ticker
// is destroyed, whether it settled or timed out.
struct FUnattendedDuringImport
{
	FUnattendedDuringImport() : bPrevious(GIsRunningUnattendedScript)
	{
		GIsRunningUnattendedScript = true;
	}
	~FUnattendedDuringImport() { GIsRunningUnattendedScript = bPrevious; }
	bool bPrevious;
};

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

// State one watch shares between the registry hook and the ticker; it lives until the ticker stops.
struct FImportWatch
{
	double Elapsed = 0.0;
	double QuietFor = 0.0;
	int32 LastCount = 0;
	TSet<FString> AddedSet;
	FCriticalSection AddedLock;
	FDelegateHandle AddedHandle;
	FTSTicker::FDelegateHandle TickerHandle;
	FUnattendedDuringImport Unattended;
};

void WatchForImport(
	TSet<FString> Before,
	FMcpFabAddResult Partial,
	TFunction<void(const FMcpFabAddResult&)> OnComplete)
{
	TSharedRef<FImportWatch> Watch = MakeShared<FImportWatch>();

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
		TEXT("AssetRegistry")).Get();
	Watch->AddedHandle = Registry.OnAssetAdded().AddLambda(
		[Before, Watch](const FAssetData& AssetData)
		{
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
			const FString Path = AssetData.GetObjectPathString();
#else
			const FString Path = AssetData.ObjectPath.ToString();
#endif
			// Skip the baseline and sub-objects: a map contributes entries like
			// Map.Map:PersistentLevel.ActorFolder_UID_..., which are parts of
			// one asset rather than assets.
			if (Before.Contains(Path) || Path.Contains(TEXT(":")))
			{
				return;
			}
			FScopeLock Lock(&Watch->AddedLock);
			Watch->AddedSet.Add(Path);
		});

	Watch->TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[Partial, OnComplete, Watch](float Delta) mutable
		{
			Watch->Elapsed += Delta;

			int32 Count;
			{
				FScopeLock Lock(&Watch->AddedLock);
				Count = Watch->AddedSet.Num();
			}
			if (Count != Watch->LastCount) { Watch->LastCount = Count; Watch->QuietFor = 0.0; }
			else if (Count > 0) { Watch->QuietFor += Delta; }

			const bool bSettled = Count > 0 && Watch->QuietFor >= SettleSeconds;
			const bool bExpired = Watch->Elapsed >= MaxWaitSeconds;
			if (!bSettled && !bExpired)
			{
				return true; // keep ticking
			}

			TArray<FString> Added;
			{
				FScopeLock Lock(&Watch->AddedLock);
				Added = Watch->AddedSet.Array();
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
			RegistryRef.OnAssetAdded().Remove(Watch->AddedHandle);
			FTSTicker::GetCoreTicker().RemoveTicker(Watch->TickerHandle);
			bOperationInFlight = false;
			OnComplete(Partial);
			return false;
		}), 1.0f);
}
} // namespace McpFabImportWatcher

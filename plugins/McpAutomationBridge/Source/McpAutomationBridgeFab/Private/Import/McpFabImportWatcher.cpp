// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabImportWatcher.h"

#include "McpFabImportOperations.h"
#include "McpFabImportScope.h"
#include "McpFabInterchange.h"
#include "McpFabLogCapture.h"

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
/** How long the registry must stay quiet, with Interchange idle, before the import is called done. */
constexpr double SettleSeconds = 6.0;
/** The most the settle waits on Interchange being busy. */
constexpr double MaxInterchangeWaitSeconds = 300.0;
/** How long after the outcome is stored each follow-up save runs. */
constexpr double LateSaveDelays[] = {15.0, 60.0};
/** Every import gets this long; a larger download gets a second more per megabyte on top. */
constexpr double BaseCeilingSeconds = 600.0;
constexpr double LongestCeilingSeconds = 7200.0;

/** A pack can be gigabytes; this is a ceiling, not an expectation. */
double CeilingSeconds(int64 DownloadBytes)
{
	const double Extra = DownloadBytes > 0 ? static_cast<double>(DownloadBytes) / 1.0e6 : 0.0;
	return FMath::Clamp(BaseCeilingSeconds + Extra, BaseCeilingSeconds, LongestCeilingSeconds);
}

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

/**
 * Saves again at each of LateSaveDelays, counted from now, and stores the result after every run. The
 * engine finishes an import after the registry went quiet (a mesh builds, a material compiles) and can
 * mark a package dirty then; one saved at the settle would otherwise stay in memory until the next
 * restart or crash lost it.
 */
void ScheduleLateSaves(const FString& OperationId, FMcpFabAddResult Result, TArray<FString> Paths,
	TFunction<void(FMcpFabAddResult&, const TArray<FString>&)> SaveAgain)
{
	struct FLate
	{
		int32 Next = 0;
		double Elapsed = 0.0;
	};
	const TSharedRef<FLate> Late = MakeShared<FLate>();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[OperationId, Result, Paths, SaveAgain, Late](float Delta) mutable
		{
			Late->Elapsed += Delta;
			if (Late->Elapsed < LateSaveDelays[Late->Next])
			{
				return true;
			}
			SaveAgain(Result, Paths);
			McpFabImportOperations::Amend(OperationId, Result);
			return ++Late->Next < static_cast<int32>(UE_ARRAY_COUNT(LateSaveDelays));
		}), 1.0f);
}
} // namespace

// State one watch shares between the registry hook and the ticker; it lives until the ticker stops.
struct FImportWatch
{
	double Elapsed = 0.0;
	double QuietFor = 0.0;
	double InterchangeWaited = 0.0;
	int32 LastCount = 0;
	TSet<FString> AddedSet;
	TSet<FString> MeshSet;
	FCriticalSection AddedLock;
	FDelegateHandle AddedHandle;
	FString FabFailure;
	FUnattendedDuringImport Unattended;
};

void WatchForImport(
	const FString& OperationId,
	TSet<FString> Before,
	FMcpFabAddResult Accepted,
	TFunction<void(FMcpFabAddResult&, TArray<FString>&)> PostImport,
	TFunction<void(FMcpFabAddResult&, const TArray<FString>&)> SaveAgain)
{
	TSharedRef<FImportWatch> Watch = MakeShared<FImportWatch>();
	McpFabLogCapture::Start();

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
		TEXT("AssetRegistry")).Get();
	const TSet<FString> OldTops = McpFabImportScope::TopFolders(Before);
	Watch->AddedHandle = Registry.OnAssetAdded().AddLambda(
		[Before, OldTops, Watch](const FAssetData& AssetData)
		{
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
			const FString Path = AssetData.GetObjectPathString();
			const FName ClassName = AssetData.AssetClassPath.GetAssetName();
#else
			const FString Path = AssetData.ObjectPath.ToString();
			const FName ClassName = AssetData.AssetClass;
#endif
			// Skip the baseline, sub-objects (a map contributes entries like
			// Map.Map:PersistentLevel.ActorFolder_UID_..., parts of one asset)
			// and what other calls create meanwhile (McpFabImportScope).
			if (Before.Contains(Path) || Path.Contains(TEXT(":")) || !McpFabImportScope::Counts(Path, OldTops))
			{
				return;
			}
			FScopeLock Lock(&Watch->AddedLock);
			Watch->AddedSet.Add(Path);
			if (ClassName == FName(TEXT("StaticMesh")) || ClassName == FName(TEXT("SkeletalMesh")))
			{
				Watch->MeshSet.Add(Path);
			}
		});

	double Ceiling = CeilingSeconds(Accepted.DownloadBytes);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[Accepted, OperationId, Ceiling, PostImport, SaveAgain, Watch](float Delta) mutable
		{
			Watch->Elapsed += Delta;

			int32 Count;
			{
				FScopeLock Lock(&Watch->AddedLock);
				Count = Watch->AddedSet.Num();
			}
			if (Count != Watch->LastCount) { Watch->LastCount = Count; Watch->QuietFor = 0.0; }
			else if (Count > 0) { Watch->QuietFor += Delta; }
			// Interchange finishes an import after it has announced the last asset: meshes build, materials
			// compile, packages are marked. Settled before then, the save ran early and the late ones stayed in
			// memory. So the registry is not quiet while Interchange works, for at most MaxInterchangeWaitSeconds
			// in case it reports itself busy for good.
			if (Count > 0 && Watch->InterchangeWaited < MaxInterchangeWaitSeconds && McpFabInterchange::IsActive())
			{
				Watch->QuietFor = 0.0;
				Watch->InterchangeWaited += Delta;
			}
			// A pack publishes no size, so its ceiling was the bare ten minutes however many gigabytes it was; while
			// Fab still shows the download it is not over, so it neither settles nor runs out (up to the longest ceiling).
			if (McpFabImportOperations::IsDownloadShowing(OperationId))
			{
				Watch->QuietFor = 0.0;
				Ceiling = FMath::Max(Ceiling, FMath::Min(Watch->Elapsed + BaseCeilingSeconds, LongestCeilingSeconds));
			}
			McpFabImportOperations::SetAssetsSoFar(OperationId, Count);

			// Fab's generic importer merges every mesh of a file into one static mesh. When the caller asked for
			// them separate, that setting is switched off on the pipelines Fab generated for this import. It
			// must land while Interchange is still translating the file, which for a scene-sized one takes
			// minutes with the game thread free, so it is tried on every tick until it takes.
			if (Accepted.MeshesSeparated.IsSet() && !Accepted.MeshesSeparated.GetValue() && McpFabInterchange::SeparateMeshes() > 0)
			{
				Accepted.MeshesSeparated = true;
				McpFabImportOperations::SetMeshesSeparated(OperationId, true);
			}

			// Fab logs a failed download, unzip or import and then cancels without another word, so
			// without the log an import that died looks like one that is merely slow.
			TArray<FString> FabLines;
			McpFabLogCapture::Take(FabLines);
			for (const FString& Line : FabLines)
			{
				McpFabImportOperations::AddFabError(OperationId, Line);
				if (Watch->FabFailure.IsEmpty() && McpFabLogCapture::IsWorkflowFailure(Line))
				{
					Watch->FabFailure = Line;
				}
			}

			const bool bFabFailed = Count == 0 && !Watch->FabFailure.IsEmpty();
			const bool bCancelled = McpFabImportOperations::IsCancelRequested(OperationId);
			const bool bSettled = Count > 0 && Watch->QuietFor >= SettleSeconds;
			const bool bExpired = Watch->Elapsed >= Ceiling;
			if (!bSettled && !bExpired && !bFabFailed && !bCancelled)
			{
				return true; // keep ticking
			}

			TArray<FString> Added;
			TArray<FString> Meshes;
			{
				FScopeLock Lock(&Watch->AddedLock);
				Added = Watch->AddedSet.Array();
				Meshes = Watch->MeshSet.Array();
			}
			Added.Sort();
			TArray<FString> Others = Added.FilterByPredicate(
				[&Meshes](const FString& Path) { return !Meshes.Contains(Path); });
			Accepted.AssetCount = Count;
			Accepted.RootPath = McpFabImportScope::CommonRoot(Added);
			Accepted.SamplePaths = McpFabImportScope::PickSamples(Meshes, Others);
			if (bCancelled)
			{
				// What had landed stays where it is, unsaved and unmoved: the caller stopped this import, so
				// it is theirs to keep or delete.
				Accepted.bTimedOut = false;
				Accepted.ErrorCode = TEXT("CANCELLED");
				Accepted.Error = Count > 0
					? FString::Printf(TEXT("Cancelled on request. %d asset(s) had already landed under %s and were left as they are."), Count, *Accepted.RootPath)
					: FString(TEXT("Cancelled on request before any asset landed."));
			}
			else if (bFabFailed)
			{
				Accepted.bTimedOut = false;
				Accepted.ErrorCode = TEXT("FAB_IMPORT_FAILED");
				Accepted.Error = FString::Printf(TEXT("Fab stopped the import and logged: %s"), *Watch->FabFailure);
			}
			else if (Count == 0)
			{
				Accepted.bTimedOut = true;
				Accepted.ErrorCode = TEXT("IMPORT_TIMED_OUT");
				Accepted.Error = FString::Printf(
					TEXT("Fab accepted the workflow but no new asset appeared within %.0f seconds."), Ceiling);
			}
			else if (bExpired)
			{
				// The ceiling was reached while packages were still streaming:
				// report what landed but mark it, so the caller does not mistake
				// a partial import for a settled one.
				Accepted.bTimedOut = true;
				Accepted.ErrorCode = TEXT("IMPORT_PARTIAL");
				Accepted.Error = FString::Printf(
					TEXT("The import was still streaming when the %.0f-second ceiling was reached; %d asset(s) landed."),
					Ceiling, Count);
			}
			else if (!Watch->FabFailure.IsEmpty())
			{
				Accepted.bTimedOut = false;
				Accepted.ErrorCode = TEXT("IMPORT_PARTIAL");
				Accepted.Error = FString::Printf(
					TEXT("%d asset(s) landed under %s, but Fab also logged a failure: %s"), Count, *Accepted.RootPath, *Watch->FabFailure);
			}

			// The hooks come off before the post-import step: moving or saving assets raises registry
			// events of its own, and none of them are this import.
			IAssetRegistry& RegistryRef = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
				TEXT("AssetRegistry")).Get();
			RegistryRef.OnAssetAdded().Remove(Watch->AddedHandle);
			McpFabLogCapture::Stop();
			if (PostImport && Count > 0 && !bCancelled)
			{
				PostImport(Accepted, Added);
			}
			McpFabImportOperations::Finish(OperationId, Accepted);
			if (SaveAgain && Count > 0 && !bCancelled)
			{
				ScheduleLateSaves(OperationId, Accepted, Added, SaveAgain);
			}
			return false;
		}), 0.25f);
}
} // namespace McpFabImportWatcher

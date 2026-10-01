// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabImportState.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"

namespace McpFabImportOperations
{
namespace
{
// What Fab's download folder for a listing shows: an unfinished download, or something finished.
struct FDownloadFolder
{
	int64 PartialBytes = 0;
	bool bPartial = false;
	bool bFinished = false;
};

// Fab's HTTP workflows stream to <cache>/<listingId>/<file>.download and rename it when done, then unzip
// beside it. Those files are the only view this adapter has of a download in flight: Fab's request object
// is private to its workflow, and list_fab_downloads reports only archives at the top of the cache.
FDownloadFolder ProbeDownloadFolder(const FString& CacheLocation, const FString& ListingId)
{
	FDownloadFolder Folder;
	const FString Root = FPaths::Combine(CacheLocation, ListingId);
	IFileManager& Files = IFileManager::Get();
	TArray<FString> Partial;
	Files.FindFiles(Partial, *(Root / TEXT("*.download")), /*Files=*/true, /*Directories=*/false);
	for (const FString& Name : Partial)
	{
		Folder.PartialBytes += FMath::Max<int64>(0, Files.FileSize(*(Root / Name)));
	}
	Folder.bPartial = Partial.Num() > 0;
	TArray<FString> Everything;
	Files.FindFiles(Everything, *(Root / TEXT("*")), /*Files=*/true, /*Directories=*/true);
	Folder.bFinished = Everything.Num() > Partial.Num();
	return Folder;
}
} // namespace

int32 PositionOf(const FOperation& Target)
{
	if (Target.State != EState::Queued)
	{
		return 0;
	}
	int32 Position = 0;
	for (const FOperation& Op : Operations())
	{
		if (Op.State == EState::Queued)
		{
			++Position;
		}
		if (&Op == &Target)
		{
			return Position;
		}
	}
	return 0;
}

void Describe(const FOperation& Op, const FString& CacheLocation, FMcpFabImportStatus& Out)
{
	Out.OperationId = Op.Id;
	Out.ListingId = Op.ListingId;
	Out.AssetsSoFar = Op.AssetsSoFar;
	Out.Result = Op.Result;
	Out.FabErrors = Op.FabErrors;
	Out.DownloadedBytes = -1;
	Out.DownloadPercent = -1.0f;
	Out.QueuePosition = PositionOf(Op);
	Out.ElapsedSeconds = (IsOpen(Op) ? FPlatformTime::Seconds() : Op.FinishedAt) - Op.StartedAt;

	switch (Op.State)
	{
	case EState::Queued:
		Out.Phase = TEXT("queued");
		break;
	case EState::Resolving:
		Out.Phase = TEXT("resolving");
		break;
	case EState::Done:
		Out.Phase = TEXT("done");
		break;
	case EState::Failed:
		Out.Phase = TEXT("failed");
		break;
	case EState::Active:
		Out.Phase = TEXT("downloading");
		if (Op.AssetsSoFar > 0)
		{
			Out.Phase = TEXT("importing");
		}
		// An unreal-engine pack downloads through BuildPatch into the project, not into this folder, so
		// only a source format can be read from disk; a pack stays `downloading` until assets appear.
		else if (!CacheLocation.IsEmpty() && Op.Result.FormatCode != TEXT("unreal-engine"))
		{
			const FDownloadFolder Folder = ProbeDownloadFolder(CacheLocation, Op.ListingId);
			if (Folder.bPartial)
			{
				Out.DownloadedBytes = Folder.PartialBytes;
			}
			else if (Folder.bFinished)
			{
				Out.Phase = TEXT("importing");
				Out.DownloadedBytes = Op.Result.DownloadBytes;
			}
		}
		break;
	}

	// Fab's own notification is the one place a download's percent shows, for a pack and a source format alike.
	McpFabDownloadProgress::FNotification Toast;
	if (Op.State == EState::Active && Out.Phase == TEXT("downloading"))
	{
		Toast = McpFabDownloadProgress::Read(ToastName(Op));
		Out.DownloadPercent = Toast.Percent;
	}
	Out.bCancellable = IsOpen(Op) && !Op.bCancelRequested && RouteFor(Op, Toast) != ECancelRoute::None;
	if (IsOpen(Op) && Op.bCancelRequested)
	{
		Out.Phase = TEXT("cancelling");
	}
}
} // namespace McpFabImportOperations

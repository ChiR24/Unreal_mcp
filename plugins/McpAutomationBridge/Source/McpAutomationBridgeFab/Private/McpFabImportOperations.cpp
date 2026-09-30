// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabImportOperations.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace McpFabImportOperations
{
namespace
{
enum class EState : uint8 { Resolving, Active, Done, Failed };

struct FOperation
{
	FString Id;
	FString ListingId;
	EState State = EState::Resolving;
	double StartedAt = 0.0;
	double FinishedAt = 0.0;
	int32 AssetsSoFar = 0;
	/** The accept-time facts, replaced by the outcome once the import ends. */
	FMcpFabAddResult Result;
	TArray<FString> FabErrors;
};

/** Enough history to read a finished import back; the oldest finished one goes first. */
constexpr int32 MaxOperations = 16;
constexpr int32 MaxFabErrors = 6;

TArray<FOperation> Operations;

bool IsRunning(const FOperation& Op)
{
	return Op.State == EState::Resolving || Op.State == EState::Active;
}

FOperation* FindById(const FString& Id)
{
	return Operations.FindByPredicate([&Id](const FOperation& Op) { return Op.Id == Id; });
}

/** What Fab's download folder for a listing shows: an unfinished download, or something finished. */
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

void Describe(const FOperation& Op, const FString& CacheLocation, FMcpFabImportStatus& Out)
{
	Out.OperationId = Op.Id;
	Out.ListingId = Op.ListingId;
	Out.AssetsSoFar = Op.AssetsSoFar;
	Out.Result = Op.Result;
	Out.FabErrors = Op.FabErrors;
	Out.DownloadedBytes = -1;
	Out.ElapsedSeconds = (IsRunning(Op) ? FPlatformTime::Seconds() : Op.FinishedAt) - Op.StartedAt;

	switch (Op.State)
	{
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
}
} // namespace

FString Begin(const FString& ListingId)
{
	if (Operations.Num() >= MaxOperations)
	{
		const int32 Oldest = Operations.IndexOfByPredicate([](const FOperation& Op) { return !IsRunning(Op); });
		if (Oldest != INDEX_NONE)
		{
			Operations.RemoveAt(Oldest);
		}
	}
	FOperation& Op = Operations.AddDefaulted_GetRef();
	Op.Id = FString::Printf(TEXT("fab-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(10).ToLower());
	Op.ListingId = ListingId;
	Op.StartedAt = FPlatformTime::Seconds();
	return Op.Id;
}

void Accept(const FString& OperationId, const FMcpFabAddResult& Accepted)
{
	if (FOperation* Op = FindById(OperationId))
	{
		Op->State = EState::Active;
		Op->Result = Accepted;
	}
}

void SetAssetsSoFar(const FString& OperationId, int32 Count)
{
	if (FOperation* Op = FindById(OperationId))
	{
		Op->AssetsSoFar = Count;
	}
}

void AddFabError(const FString& OperationId, const FString& Line)
{
	FOperation* Op = FindById(OperationId);
	if (Op != nullptr && Op->FabErrors.Num() < MaxFabErrors)
	{
		Op->FabErrors.AddUnique(Line);
	}
}

void Finish(const FString& OperationId, const FMcpFabAddResult& Outcome)
{
	if (FOperation* Op = FindById(OperationId))
	{
		Op->Result = Outcome;
		Op->State = Outcome.ErrorCode.IsEmpty() ? EState::Done : EState::Failed;
		Op->FinishedAt = FPlatformTime::Seconds();
		Op->AssetsSoFar = FMath::Max(Op->AssetsSoFar, Outcome.AssetCount);
	}
}

bool FindRunning(const FString& CacheLocation, FMcpFabImportStatus& OutStatus)
{
	for (int32 Index = Operations.Num() - 1; Index >= 0; --Index)
	{
		if (IsRunning(Operations[Index]))
		{
			Describe(Operations[Index], CacheLocation, OutStatus);
			return true;
		}
	}
	return false;
}

bool Find(const FString& Key, const FString& CacheLocation, FMcpFabImportStatus& OutStatus)
{
	if (const FOperation* ById = FindById(Key))
	{
		Describe(*ById, CacheLocation, OutStatus);
		return true;
	}
	for (int32 Index = Operations.Num() - 1; Index >= 0; --Index)
	{
		if (Operations[Index].ListingId == Key)
		{
			Describe(Operations[Index], CacheLocation, OutStatus);
			return true;
		}
	}
	return false;
}
} // namespace McpFabImportOperations

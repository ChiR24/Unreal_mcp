// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "McpFabImportOperations.h"
#include "McpFabImportState.h"

#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"

namespace McpFabImportOperations
{
namespace
{
// Enough history to read a finished import back; the oldest finished one goes first.
constexpr int32 MaxOperations = 16;
constexpr int32 MaxFabErrors = 6;

bool bPumpScheduled = false;

FOperation* FindById(const FString& Id)
{
	return Operations().FindByPredicate([&Id](const FOperation& Op) { return Op.Id == Id; });
}

// Starts the oldest queued add once nothing is running.
void StartNext()
{
	for (const FOperation& Op : Operations())
	{
		if (IsRunning(Op))
		{
			return;
		}
	}
	FOperation* Next = Operations().FindByPredicate([](const FOperation& Op) { return Op.State == EState::Queued; });
	if (Next == nullptr)
	{
		return;
	}
	Next->State = EState::Resolving;
	TFunction<void()> Launch = MoveTemp(Next->Launch);
	Next->Launch = nullptr;
	// The closure may finish operations and add new ones: nothing here touches Next after it runs.
	if (Launch)
	{
		Launch();
	}
}

// Starts the next add a moment after an import ends, outside the ticker that ended it.
void SchedulePump()
{
	if (bPumpScheduled)
	{
		return;
	}
	bPumpScheduled = true;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float) -> bool
	{
		bPumpScheduled = false;
		StartNext();
		return false;
	}), 0.1f);
}
} // namespace

TArray<FOperation>& Operations()
{
	static TArray<FOperation> All;
	return All;
}

FString Begin(const FString& ListingId)
{
	if (Operations().Num() >= MaxOperations)
	{
		const int32 Oldest = Operations().IndexOfByPredicate([](const FOperation& Op) { return !IsOpen(Op); });
		if (Oldest != INDEX_NONE)
		{
			Operations().RemoveAt(Oldest);
		}
	}
	FOperation& Op = Operations().AddDefaulted_GetRef();
	Op.Id = FString::Printf(TEXT("fab-%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(10).ToLower());
	Op.ListingId = ListingId;
	Op.StartedAt = FPlatformTime::Seconds();
	return Op.Id;
}

void Enqueue(const FString& OperationId, TFunction<void()> Launch)
{
	if (FOperation* Op = FindById(OperationId))
	{
		Op->State = EState::Queued;
		Op->Launch = MoveTemp(Launch);
	}
	// Normally an import is running and its end starts this one. If it ended in the moment before
	// this add arrived, nothing else would.
	SchedulePump();
}

int32 QueuedCount()
{
	int32 Count = 0;
	for (const FOperation& Op : Operations())
	{
		Count += Op.State == EState::Queued ? 1 : 0;
	}
	return Count;
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

void SetMeshesSeparated(const FString& OperationId, bool bSeparated)
{
	if (FOperation* Op = FindById(OperationId))
	{
		Op->Result.MeshesSeparated = bSeparated;
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
	SchedulePump();
}

bool FindRunning(const FString& CacheLocation, FMcpFabImportStatus& OutStatus)
{
	for (int32 Index = Operations().Num() - 1; Index >= 0; --Index)
	{
		if (IsRunning(Operations()[Index]))
		{
			Describe(Operations()[Index], CacheLocation, OutStatus);
			return true;
		}
	}
	return false;
}

bool FindOpenByListing(const FString& ListingId, const FString& CacheLocation, FMcpFabImportStatus& OutStatus)
{
	for (int32 Index = Operations().Num() - 1; Index >= 0; --Index)
	{
		if (Operations()[Index].ListingId == ListingId && IsOpen(Operations()[Index]))
		{
			Describe(Operations()[Index], CacheLocation, OutStatus);
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
	for (int32 Index = Operations().Num() - 1; Index >= 0; --Index)
	{
		if (Operations()[Index].ListingId == Key)
		{
			Describe(Operations()[Index], CacheLocation, OutStatus);
			return true;
		}
	}
	return false;
}

void ListQueue(const FString& CacheLocation, TArray<FMcpFabImportStatus>& OutQueue)
{
	for (const FOperation& Op : Operations())
	{
		if (IsRunning(Op))
		{
			Describe(Op, CacheLocation, OutQueue.AddDefaulted_GetRef());
		}
	}
	for (const FOperation& Op : Operations())
	{
		if (Op.State == EState::Queued)
		{
			Describe(Op, CacheLocation, OutQueue.AddDefaulted_GetRef());
		}
	}
}
} // namespace McpFabImportOperations

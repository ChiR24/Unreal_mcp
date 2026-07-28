// McpTaskStore.cpp — Task 44: bounded, session-partitioned MCP task retention.

#include "MCP/Primitives/McpTaskStore.h"

#include "HAL/PlatformTime.h"
#include "Math/NumericLimits.h"
#include "Misc/Guid.h"

const TCHAR* McpTaskStatusToString(EMcpTaskStatus Status)
{
	switch (Status)
	{
	case EMcpTaskStatus::Working: return TEXT("working");
	case EMcpTaskStatus::InputRequired: return TEXT("input_required");
	case EMcpTaskStatus::Completed: return TEXT("completed");
	case EMcpTaskStatus::Failed: return TEXT("failed");
	case EMcpTaskStatus::Cancelled: return TEXT("cancelled");
	}
	return TEXT("working");
}

bool McpTaskStatusIsTerminal(EMcpTaskStatus Status)
{
	return Status == EMcpTaskStatus::Completed || Status == EMcpTaskStatus::Failed
		|| Status == EMcpTaskStatus::Cancelled;
}

FMcpTaskStore::FMcpTaskStore() : FMcpTaskStore(FMcpTaskStoreConfig())
{
}

FMcpTaskStore::FMcpTaskStore(const FMcpTaskStoreConfig& InConfig) : Config(InConfig)
{
	// Milliseconds on the FDateTime scale, so a stored stamp converts straight to
	// the ISO 8601 string MCP requires. Tests replace this with a fake clock.
	Clock = []() -> int64
	{
		return FDateTime::UtcNow().GetTicks() / ETimespan::TicksPerMillisecond;
	};
}

void FMcpTaskStore::SetClock(TFunction<int64()> InClock)
{
	FScopeLock Lock(&Mutex);
	if (InClock) Clock = MoveTemp(InClock);
}

FString FMcpTaskStore::PartitionKey(const FString& SessionId)
{
	return SessionId.IsEmpty() ? TEXT("anonymous") : (TEXT("session:") + SessionId);
}

int64 FMcpTaskStore::Now() const
{
	return Clock ? Clock() : 0;
}

int64 FMcpTaskStore::ClampTtl(bool bHasRequestedTtl, int64 RequestedTtlMs) const
{
	// A null/absent TTL asks for an unlimited lifetime. A bounded store cannot
	// offer that, and the MCP contract explicitly allows the implementation to
	// override the request as long as the APPLIED ttl is what it reports back.
	if (!bHasRequestedTtl) return Config.DefaultTtlMs;
	if (RequestedTtlMs <= 0) return Config.DefaultTtlMs;
	return FMath::Min(RequestedTtlMs, Config.MaxTtlMs);
}

void FMcpTaskStore::EvictExpiredLocked(TMap<FString, FEntry>& Partition)
{
	const int64 NowMs = Now();
	TArray<FString> Expired;
	for (const TPair<FString, FEntry>& Pair : Partition)
	{
		if (Pair.Value.ExpiresAtMs <= NowMs) Expired.Add(Pair.Key);
	}
	for (const FString& Key : Expired) Partition.Remove(Key);
}

bool FMcpTaskStore::MakeRoomLocked(TMap<FString, FEntry>& Partition)
{
	// ONLY a terminal task is ever evicted: dropping one that is still running
	// would make a live handle vanish from under a polling client and silently
	// discard a result that was still going to be produced. Fail closed instead.
	while (Partition.Num() >= Config.MaxTasksPerSession)
	{
		FString OldestKey;
		int64 OldestSeq = TNumericLimits<int64>::Max();
		for (const TPair<FString, FEntry>& Pair : Partition)
		{
			if (!McpTaskStatusIsTerminal(Pair.Value.Record.Status)) continue;
			if (Pair.Value.Seq < OldestSeq)
			{
				OldestSeq = Pair.Value.Seq;
				OldestKey = Pair.Key;
			}
		}
		if (OldestKey.IsEmpty()) return false;
		Partition.Remove(OldestKey);
	}
	return true;
}

FMcpTaskStore::FEntry* FMcpTaskStore::FindLiveLocked(
	const FString& SessionId, const FString& TaskId)
{
	// Session-scoped: the id is only resolved INSIDE the caller's partition, so
	// isolation is structural rather than a check that could be forgotten.
	TMap<FString, FEntry>* Partition = Partitions.Find(PartitionKey(SessionId));
	if (!Partition) return nullptr;
	EvictExpiredLocked(*Partition);
	return Partition->Find(TaskId);
}

EMcpTaskStoreError FMcpTaskStore::CreateTask(
	const FString& SessionId, bool bHasRequestedTtl, int64 RequestedTtlMs,
	FMcpTaskRecord& OutTask)
{
	FScopeLock Lock(&Mutex);
	TMap<FString, FEntry>& Partition = Partitions.FindOrAdd(PartitionKey(SessionId));
	EvictExpiredLocked(Partition);
	if (!MakeRoomLocked(Partition)) return EMcpTaskStoreError::AtCapacity;

	const int64 NowMs = Now();
	FEntry Entry;
	Entry.Seq = NextSeq++;
	Entry.Record.TaskId = FGuid::NewGuid().ToString(EGuidFormats::DigitsLower);
	Entry.Record.Status = EMcpTaskStatus::Working;
	Entry.Record.TtlMs = ClampTtl(bHasRequestedTtl, RequestedTtlMs);
	Entry.Record.CreatedAtMs = NowMs;
	Entry.Record.LastUpdatedAtMs = NowMs;
	Entry.ExpiresAtMs = NowMs + Entry.Record.TtlMs;
	OutTask = Entry.Record;
	Partition.Add(Entry.Record.TaskId, MoveTemp(Entry));
	return EMcpTaskStoreError::None;
}

EMcpTaskStoreError FMcpTaskStore::GetTask(
	const FString& SessionId, const FString& TaskId, FMcpTaskRecord& OutTask)
{
	FScopeLock Lock(&Mutex);
	const FEntry* Entry = FindLiveLocked(SessionId, TaskId);
	if (!Entry) return EMcpTaskStoreError::NotFound;
	OutTask = Entry->Record;
	return EMcpTaskStoreError::None;
}

EMcpTaskStoreError FMcpTaskStore::StoreResult(
	const FString& SessionId, const FString& TaskId, EMcpTaskStatus Status,
	const TSharedPtr<FJsonObject>& Result)
{
	FScopeLock Lock(&Mutex);
	FEntry* Entry = FindLiveLocked(SessionId, TaskId);
	if (!Entry) return EMcpTaskStoreError::NotFound;
	if (McpTaskStatusIsTerminal(Entry->Record.Status)) return EMcpTaskStoreError::AlreadyTerminal;
	Entry->Result = Result;
	Entry->Record.Status = Status;
	Entry->Record.LastUpdatedAtMs = Now();
	return EMcpTaskStoreError::None;
}

EMcpTaskStoreError FMcpTaskStore::GetResult(
	const FString& SessionId, const FString& TaskId, TSharedPtr<FJsonObject>& OutResult)
{
	FScopeLock Lock(&Mutex);
	const FEntry* Entry = FindLiveLocked(SessionId, TaskId);
	if (!Entry) return EMcpTaskStoreError::NotFound;
	if (!Entry->Result.IsValid()) return EMcpTaskStoreError::NotFound;
	OutResult = Entry->Result;
	return EMcpTaskStoreError::None;
}

EMcpTaskStoreError FMcpTaskStore::UpdateStatus(
	const FString& SessionId, const FString& TaskId, EMcpTaskStatus Status,
	const FString& StatusMessage)
{
	FScopeLock Lock(&Mutex);
	FEntry* Entry = FindLiveLocked(SessionId, TaskId);
	if (!Entry) return EMcpTaskStoreError::NotFound;
	if (McpTaskStatusIsTerminal(Entry->Record.Status)) return EMcpTaskStoreError::AlreadyTerminal;
	Entry->Record.Status = Status;
	Entry->Record.LastUpdatedAtMs = Now();
	if (!StatusMessage.IsEmpty()) Entry->Record.StatusMessage = StatusMessage;
	return EMcpTaskStoreError::None;
}

void FMcpTaskStore::ListTasks(const FString& SessionId, TArray<FMcpTaskRecord>& OutTasks)
{
	FScopeLock Lock(&Mutex);
	OutTasks.Reset();
	TMap<FString, FEntry>* Partition = Partitions.Find(PartitionKey(SessionId));
	if (!Partition) return;
	EvictExpiredLocked(*Partition);
	TArray<const FEntry*> Ordered;
	for (const TPair<FString, FEntry>& Pair : *Partition) Ordered.Add(&Pair.Value);
	Ordered.Sort([](const FEntry& A, const FEntry& B) { return A.Seq < B.Seq; });
	for (const FEntry* Entry : Ordered) OutTasks.Add(Entry->Record);
}

void FMcpTaskStore::CloseSession(const FString& SessionId)
{
	FScopeLock Lock(&Mutex);
	Partitions.Remove(PartitionKey(SessionId));
}

void FMcpTaskStore::Clear()
{
	FScopeLock Lock(&Mutex);
	Partitions.Empty();
}

int32 FMcpTaskStore::SessionSize(const FString& SessionId)
{
	FScopeLock Lock(&Mutex);
	TMap<FString, FEntry>* Partition = Partitions.Find(PartitionKey(SessionId));
	if (!Partition) return 0;
	EvictExpiredLocked(*Partition);
	return Partition->Num();
}

int32 FMcpTaskStore::TotalSize()
{
	FScopeLock Lock(&Mutex);
	int32 Total = 0;
	for (TPair<FString, TMap<FString, FEntry>>& Pair : Partitions)
	{
		EvictExpiredLocked(Pair.Value);
		Total += Pair.Value.Num();
	}
	return Total;
}

// McpTaskStore.h — Task 44: the per-session BOUNDED task store behind MCP Tasks.
//
// Native mirror of src/server/mcp-primitives/bounded-task-store.ts. The two must
// agree on all four properties or the transports diverge:
//   * a hard per-session cap that is never exceeded;
//   * eviction that removes ONLY a terminal task, oldest first, and REFUSES
//     rather than dropping a task that is still running;
//   * TTL measured from creation and clamped to a ceiling, so the "unlimited
//     lifetime" a null TTL asks for is never granted;
//   * session isolation — a task id is only ever resolved inside its owner's
//     partition, so another session cannot read, cancel, or evict it, and
//     cannot distinguish "not yours" from "never existed".
// Exactly one terminal transition is accepted per task, so a late or duplicate
// result can never overwrite the one already published.
//
// The clock is injectable so expiry is provable in a test without sleeping.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

enum class EMcpTaskStatus : uint8
{
	Working,
	InputRequired,
	Completed,
	Failed,
	Cancelled
};

const TCHAR* McpTaskStatusToString(EMcpTaskStatus Status);
bool McpTaskStatusIsTerminal(EMcpTaskStatus Status);

enum class EMcpTaskStoreError : uint8
{
	None,
	AtCapacity,
	NotFound,
	AlreadyTerminal
};

struct FMcpTaskRecord
{
	FString TaskId;
	EMcpTaskStatus Status = EMcpTaskStatus::Working;
	int64 TtlMs = 0;
	int64 CreatedAtMs = 0;
	int64 LastUpdatedAtMs = 0;
	FString StatusMessage;
};

struct FMcpTaskStoreConfig
{
	int32 MaxTasksPerSession = 32;
	int64 DefaultTtlMs = 5 * 60 * 1000;
	int64 MaxTtlMs = 30 * 60 * 1000;
};

class FMcpTaskStore
{
public:
	FMcpTaskStore();
	explicit FMcpTaskStore(const FMcpTaskStoreConfig& InConfig);

	// Tests drive a fake clock through this; production leaves the wall clock.
	void SetClock(TFunction<int64()> InClock);

	EMcpTaskStoreError CreateTask(
		const FString& SessionId, bool bHasRequestedTtl, int64 RequestedTtlMs,
		FMcpTaskRecord& OutTask);
	EMcpTaskStoreError GetTask(
		const FString& SessionId, const FString& TaskId, FMcpTaskRecord& OutTask);
	EMcpTaskStoreError StoreResult(
		const FString& SessionId, const FString& TaskId, EMcpTaskStatus Status,
		const TSharedPtr<FJsonObject>& Result);
	EMcpTaskStoreError GetResult(
		const FString& SessionId, const FString& TaskId, TSharedPtr<FJsonObject>& OutResult);
	EMcpTaskStoreError UpdateStatus(
		const FString& SessionId, const FString& TaskId, EMcpTaskStatus Status,
		const FString& StatusMessage);
	void ListTasks(const FString& SessionId, TArray<FMcpTaskRecord>& OutTasks);

	void CloseSession(const FString& SessionId);
	void Clear();
	int32 SessionSize(const FString& SessionId);
	int32 TotalSize();

private:
	struct FEntry
	{
		FMcpTaskRecord Record;
		int64 Seq = 0;
		int64 ExpiresAtMs = 0;
		TSharedPtr<FJsonObject> Result;
	};

	// Named sessions are prefixed so no client-chosen id can collide with the
	// anonymous partition a transport without sessions uses.
	static FString PartitionKey(const FString& SessionId);
	int64 Now() const;
	int64 ClampTtl(bool bHasRequestedTtl, int64 RequestedTtlMs) const;
	void EvictExpiredLocked(TMap<FString, FEntry>& Partition);
	bool MakeRoomLocked(TMap<FString, FEntry>& Partition);
	FEntry* FindLiveLocked(const FString& SessionId, const FString& TaskId);

	FMcpTaskStoreConfig Config;
	TFunction<int64()> Clock;
	mutable FCriticalSection Mutex;
	TMap<FString, TMap<FString, FEntry>> Partitions;
	int64 NextSeq = 0;
};

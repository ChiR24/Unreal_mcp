#include "MCP/DynamicTools/McpTaskResults.h"
#include "Misc/ScopeLock.h"

FMcpTaskResults::FTask& FMcpTaskResults::FindOrAddLocked(const FString& TaskId)
{
	if (FTask* Found = Entries.Find(TaskId))
	{
		return *Found;
	}
	Order.Add(TaskId);
	while (Order.Num() > MaxKept)
	{
		Entries.Remove(Order[0]);
		Order.RemoveAt(0);
	}
	return Entries.Add(TaskId);
}

void FMcpTaskResults::NoteRunning(const FString& TaskId, const FString& Principal, const FDateTime& CreatedAt)
{
	FScopeLock Lock(&Mutex);
	FTask& Task = FindOrAddLocked(TaskId);
	Task.Principal = Principal;
	Task.CreatedAt = CreatedAt;
	Task.UpdatedAt = FDateTime::UtcNow();
}

void FMcpTaskResults::NoteDone(
	const FString& TaskId, const TSharedPtr<FJsonObject>& Outcome, const TSharedPtr<FJsonObject>& ToolResult)
{
	FScopeLock Lock(&Mutex);
	FTask& Task = FindOrAddLocked(TaskId);
	Task.Outcome = Outcome;
	Task.ToolResult = ToolResult;
	Task.UpdatedAt = FDateTime::UtcNow();
}

void FMcpTaskResults::NoteCancelled(const FString& TaskId)
{
	FScopeLock Lock(&Mutex);
	FTask* Task = Entries.Find(TaskId);
	if (Task != nullptr && !Task->Outcome.IsValid())
	{
		Task->bCancelled = true;
		Task->UpdatedAt = FDateTime::UtcNow();
	}
}

bool FMcpTaskResults::Find(const FString& TaskId, const FString& Principal, FTask& OutTask) const
{
	FScopeLock Lock(&Mutex);
	const FTask* Entry = Entries.Find(TaskId);
	// Case-sensitive: FString's != ignores case, which would let one principal read another's task.
	if (Entry == nullptr || !Entry->Principal.Equals(Principal, ESearchCase::CaseSensitive))
	{
		return false;
	}
	OutTask = *Entry;
	return true;
}

#include "MCP/DynamicTools/McpTaskResults.h"
#include "Misc/ScopeLock.h"

FMcpTaskResults::FEntry& FMcpTaskResults::FindOrAddLocked(const FString& TaskId)
{
	if (FEntry* Found = Entries.Find(TaskId))
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

void FMcpTaskResults::NoteRunning(const FString& TaskId, const FString& Principal)
{
	FScopeLock Lock(&Mutex);
	FindOrAddLocked(TaskId).Principal = Principal;
}

void FMcpTaskResults::NoteDone(const FString& TaskId, const TSharedPtr<FJsonObject>& Outcome)
{
	FScopeLock Lock(&Mutex);
	FindOrAddLocked(TaskId).Outcome = Outcome;
}

bool FMcpTaskResults::Find(const FString& TaskId, const FString& Principal, TSharedPtr<FJsonObject>& OutOutcome) const
{
	FScopeLock Lock(&Mutex);
	const FEntry* Entry = Entries.Find(TaskId);
	if (Entry == nullptr || Entry->Principal != Principal)
	{
		return false;
	}
	OutOutcome = Entry->Outcome;
	return true;
}

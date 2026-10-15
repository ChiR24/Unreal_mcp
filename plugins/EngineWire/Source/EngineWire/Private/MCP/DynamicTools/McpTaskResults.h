#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HAL/CriticalSection.h"

/**
 * Calls that answered "still running" (FMcpNativeTransport::AnswerStillRunning) and, once done, their outcome, read
 * back by manage_tools get_task_result: past that answer the call's own reply went only to the log. Keyed by the task
 * id the answer gave, readable only by the principal that sent the call, and capped at the last MaxKept calls.
 * Mirrors src/server/gateway/gateway-task-results.ts on the TypeScript door.
 */
class FMcpTaskResults
{
public:
	static constexpr int32 MaxKept = 32;

	/** The call answered "still running" for this principal. */
	void NoteRunning(const FString& TaskId, const FString& Principal);

	/** The call has finished; either note may come first. */
	void NoteDone(const FString& TaskId, const TSharedPtr<FJsonObject>& Outcome);

	/** False when the task is not kept or belongs to another principal; OutOutcome stays null while it runs. */
	bool Find(const FString& TaskId, const FString& Principal, TSharedPtr<FJsonObject>& OutOutcome) const;

private:
	struct FEntry
	{
		FString Principal;
		TSharedPtr<FJsonObject> Outcome;
	};

	FEntry& FindOrAddLocked(const FString& TaskId);

	mutable FCriticalSection Mutex;
	TMap<FString, FEntry> Entries;
	TArray<FString> Order;
};

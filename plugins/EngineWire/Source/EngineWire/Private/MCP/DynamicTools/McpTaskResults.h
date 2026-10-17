#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HAL/CriticalSection.h"

/**
 * Calls that answered "still running" (FMcpNativeTransport::AnswerStillRunning) and, once done, their outcome, read
 * back by manage_tools get_task_result and by the tasks extension's tasks/get: past that answer the call's own reply
 * went only to the log. Keyed by the task id the answer gave, readable only by the principal that sent the call, and
 * capped at the last MaxKept calls. Mirrors src/server/gateway/gateway-task-results.ts on the TypeScript door.
 */
class FMcpTaskResults
{
public:
	static constexpr int32 MaxKept = 32;

	struct FTask
	{
		FString Principal;
		TSharedPtr<FJsonObject> Outcome;     // get_task_result's outcome, once done
		TSharedPtr<FJsonObject> ToolResult;  // the call's own result, once done (tasks/get)
		FDateTime CreatedAt;
		FDateTime UpdatedAt;
		bool bCancelled = false;             // cancelled through tasks/cancel before it ran
	};

	/** The call, started at CreatedAt, answered "still running" for this principal. */
	void NoteRunning(const FString& TaskId, const FString& Principal, const FDateTime& CreatedAt);

	/** The call has finished; either note may come first. */
	void NoteDone(const FString& TaskId, const TSharedPtr<FJsonObject>& Outcome, const TSharedPtr<FJsonObject>& ToolResult);

	/** The call was cancelled before it finished; a finished one stays finished. */
	void NoteCancelled(const FString& TaskId);

	/** False when the task is not kept or belongs to another principal; OutTask.Outcome stays null while it runs. */
	bool Find(const FString& TaskId, const FString& Principal, FTask& OutTask) const;

private:
	FTask& FindOrAddLocked(const FString& TaskId);

	mutable FCriticalSection Mutex;
	TMap<FString, FTask> Entries;
	TArray<FString> Order;
};

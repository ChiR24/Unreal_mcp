// McpTaskMethods.h — Task 44: the native MCP Tasks surface (2025-11-25).
//
// Owns the bounded store and answers tasks/get, tasks/list, tasks/cancel and
// tasks/result, plus the one way a task is created here: a task-augmented
// tools/call that is a SAFE READ-ONLY CHECKPOINT.
//
// Mirrors src/server/mcp-primitives/task-checkpoint.ts. Only the read-only
// gateway operations may be task-augmented. Cancelling a task cannot interrupt
// work already dispatched to the editor, so a mutating operation is never handed
// back as a cancellable handle; it is refused BEFORE anything runs, with an
// executable pointer at the call that does work.

#pragma once

#include "CoreMinimal.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"
#include "MCP/Primitives/McpTaskStore.h"

// The gateway operations that may be task-augmented, in stable order.
const TArray<FString>& McpTaskCheckpointOperations();
bool McpTaskCheckpointOperationAllowed(const FString& Operation);

extern const TCHAR* const McpTaskCheckpointRefusedCode;

// One task rendered as the MCP `Task` object. `ttl` is always a number: a
// bounded store never grants the unlimited lifetime a null ttl asks for.
TSharedPtr<FJsonObject> McpTaskRecordToJson(const FMcpTaskRecord& Record);

class FMcpTaskSurface
{
public:
	// Answer a tasks/* method. Returns false — filling nothing — when Method is
	// not ours, so the caller still reports method-not-found for anything else.
	bool HandleMethod(
		const FString& Method, const TSharedPtr<FJsonObject>& Params,
		const TSharedPtr<FJsonValue>& Id, const FString& SessionId, FString& OutBody);

	// Answer a task-augmented tools/call. Always fills OutBody: either the
	// refusal, or the created task after its read-only result has been retained.
	void HandleToolCallCheckpoint(
		const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments,
		const TSharedPtr<FJsonObject>& TaskCreation, const TSharedPtr<FJsonValue>& Id,
		const FString& SessionId, FMcpToolEnabledPredicate IsToolEnabled, FString& OutBody);

	void CloseSession(const FString& SessionId);

private:
	FMcpTaskStore Store;
};

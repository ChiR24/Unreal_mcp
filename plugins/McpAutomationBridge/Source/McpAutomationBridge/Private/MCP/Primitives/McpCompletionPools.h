// McpCompletionPools.h
// Task 38 lane B: the concrete completion candidate pools the native transport
// feeds into McpCompleteFromPool, mirroring completion-sources.ts. The capability
// pool is every canonical id plus its legacy `parentTool.action` form (each tagged
// with the canonical id for capability-scoped filtering); the project-handle pool
// is the friendly class-alias keys. Both read only the immutable capability store
// and a static alias table: no editor scan, no socket, no raw filesystem path.
#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "MCP/Primitives/McpCompletionProvider.h"

const TArray<FMcpCompletionCandidate>& McpCapabilityCompletionPool();

const TArray<FMcpCompletionCandidate>& McpProjectHandleCompletionPool();


TSet<FString> McpEnabledCapabilityIds(TFunctionRef<bool(const FString&)> IsParentEnabled);

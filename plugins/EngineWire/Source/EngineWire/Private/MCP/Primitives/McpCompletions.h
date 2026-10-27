// McpCompletions.h
// completion/complete over a closed set of slots (mirrors completions.ts):
// capability ids, knowledge topics, content roots and the enum-valued prompt
// arguments. Every pool is in-memory; nothing scans the editor or host paths.
#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

struct FMcpCompletionResult
{
	TArray<FString> Values;
	int32 Total = 0;
	bool bHasMore = false;
};

FMcpCompletionResult McpComplete(
	const FString& RefType, const FString& RefId, const FString& ArgumentName, const FString& Prefix,
	TFunctionRef<bool(const FString&)> IsParentEnabled);

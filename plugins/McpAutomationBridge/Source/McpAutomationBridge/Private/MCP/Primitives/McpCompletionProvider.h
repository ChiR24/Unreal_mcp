// McpCompletionProvider.h
// Task 33 primitive (native mirror): bounded MCP completions. Native counterpart
// of src/server/mcp-primitives/completions/. It carries NO transport wiring,
// executes NOTHING, scans NO editor; it defines the bounded budgets, the typed
// guidance codes, the completable-slot table, the safety gate, the candidate and
// result shapes, the bounded enum value sets, and the deterministic ranking used
// by the completion/complete method. Task 37 supplies the capability/handle pools
// (from the native registry and safe caches) and the session enabled set, then
// wires this into the protocol. The slot table, enum data, and pure logic live
// in McpCompletionProvider.cpp; the source-contract test asserts TS/native parity.
#pragma once

#include "CoreMinimal.h"

// Bounded budgets. Mirror MAX_COMPLETION_ITEMS / MAX_COMPLETION_BYTES /
// MAX_PREFIX_LENGTH from completion-types.ts.
inline constexpr int32 McpMaxCompletionItems = 100;
inline constexpr int32 McpMaxCompletionBytes = 8192;
inline constexpr int32 McpMaxCompletionPrefixLength = 128;

// Stable guidance codes for a safe-empty outcome. Mirror COMPLETION_GUIDANCE_CODES
// so a refusal or an empty match is never mistaken for a suggestion.
namespace McpCompletionGuidance
{
	inline const TCHAR* SecretField = TEXT("COMPLETION_SECRET_FIELD");
	inline const TCHAR* DestructiveField = TEXT("COMPLETION_DESTRUCTIVE_FIELD");
	inline const TCHAR* UnboundedPrefix = TEXT("COMPLETION_UNBOUNDED_PREFIX");
	inline const TCHAR* UnboundedPath = TEXT("COMPLETION_UNBOUNDED_PATH");
	inline const TCHAR* Unavailable = TEXT("COMPLETION_UNAVAILABLE");
	inline const TCHAR* NoMatch = TEXT("COMPLETION_NO_MATCH");
}

// A completable slot: ties one (ref, argument) pair to a bounded candidate pool.
// Mirror CompletionSlot. Kind is "capability" | "enum" | "project-handle".
struct FMcpCompletionSlot
{
	FString RefType;        // "ref/prompt" or "ref/resource"
	FString RefId;          // prompt name or resource template uri
	FString ArgumentName;
	FString Kind;
	bool bCapabilityScoped = false;
};

// A completion candidate. Mirror CompletionCandidate. CapabilityId is set for
// capability/legacy candidates so a capability-scoped slot filters by the enabled set.
struct FMcpCompletionCandidate
{
	FString Value;
	FString Kind;           // "capability" | "legacy-id" | "enum" | "project-handle"
	FString CapabilityId;
};

// The bounded completion payload. Mirror CompletionResult.
struct FMcpCompletionResult
{
	TArray<FString> Values;
	int32 Total = 0;
	bool bHasMore = false;
};

// The provider outcome: the wire payload plus an optional guidance code. A
// safe-empty outcome always carries a guidance code; a matched outcome leaves it
// empty. Mirror CompletionOutcome.
struct FMcpCompletionOutcome
{
	FMcpCompletionResult Result;
	FString GuidanceCode;
};

// The closed slot registry and its lookup. Mirror COMPLETION_SLOTS / resolveSlot.
const TArray<FMcpCompletionSlot>& McpCompletionSlots();
const FMcpCompletionSlot* McpResolveCompletionSlot(const FString& RefType, const FString& RefId, const FString& ArgumentName);

// The bounded enum value set for one enum slot, or empty. Mirror enumCandidates.
TArray<FString> McpCompletionEnumValues(const FMcpCompletionSlot& Slot);

// Classify an argument name+value as unsafe; an empty result means safe. Mirror
// classifyUnsafe (secret/destructive name, raw host path/traversal value).
FString McpClassifyUnsafeCompletion(const FString& ArgumentName, const FString& Value);

// Deterministic rank + prefix filter (tier ladder + lexicographic tiebreak).
// Mirror rankCandidates.
TArray<FMcpCompletionCandidate> McpRankCompletionCandidates(const TArray<FMcpCompletionCandidate>& Pool, const FString& Prefix);

// Cap ranked candidates to the item and serialized-byte budgets. Mirror applyBudget.
FMcpCompletionResult McpApplyCompletionBudget(const TArray<FMcpCompletionCandidate>& Ranked);

// The full fail-closed orchestration. Task 37 supplies the capability and project
// handle pools and the session enabled set. Mirror complete().
FMcpCompletionOutcome McpCompleteFromPool(
	const FString& RefType,
	const FString& RefId,
	const FString& ArgumentName,
	const FString& Value,
	const TArray<FMcpCompletionCandidate>& CapabilityPool,
	const TArray<FMcpCompletionCandidate>& ProjectHandlePool,
	const TSet<FString>& EnabledCapabilityIds);

// McpNativeGatewaySearch.h — deterministic capability search for the unreal gateway
//
// Mirrors the TypeScript discovery reference exactly: same filters, same match
// reasons and weights, same total order, same paging, same byte budget, same
// guided errors. Discovery fixtures from both surfaces are diffed byte-for-byte,
// so any divergence here is a test failure rather than a silent drift.
//
// Reads only the generated capability store. There is no alternate catalog: an
// unavailable store yields a typed error, never substituted metadata.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpCapabilityStore;

/** Runtime capability probe: is this parent tool currently exposed to the session? */
using FMcpToolEnabledPredicate = TFunctionRef<bool(const FString&)>;

struct FMcpDiscoveryQuery
{
	FString Query;
	FString Domain;
	FString Family;
	FString Tool;
	FString Action;
	FString Param;
	FString Effect;
	bool bHasDomain = false;
	bool bHasFamily = false;
	bool bHasTool = false; // search only: filter by parent tool, as the TS gateway does
	bool bHasEffect = false;
	bool bHasAction = false;
	bool bHasParam = false;
	int32 Limit = 12;
	int32 Offset = 0;
	int32 MaxBytes = 0; // 0 -> McpMaxResultBytes; the schema accepted maxBytes but native ignored it (dogfood #3)
};

/** Search default/maximum budgets, shared with the gateway tool schema. */
constexpr int32 McpSearchDefaultLimit = 12;
constexpr int32 McpSearchMaxLimit = 25;

/**
 * Per-matched-word score bonus for multi-word queries. Ranks a record that
 * covers more of the query above one that covers less, so a natural phrase
 * ("create new level map") surfaces the capability matching the most words
 * rather than returning nothing at all.
 */
constexpr int32 McpSearchWordCoverageBonus = 5;
/**
 * When the query names every word of a record's OWN action (two words or more)
 * and the record matches every query word: "delete blueprint graph node" is
 * delete_node, where a folded record's aliases used to out-score it word by word.
 */
constexpr int32 McpSearchActionCoveredBonus = 50;
/**
 * When the query opens with a read word ("get actor location", "what is in this
 * folder"), a matching record whose effect is read ranks ahead: set_transform used
 * to answer "get actor location". Only reorders records that already matched.
 */
constexpr int32 McpSearchReadIntentBonus = 40;
constexpr int32 McpDescribeDefaultLimit = 20;
constexpr int32 McpDescribeMaxLimit = 50;
// Search result byte budget (24 KB); a page that would pass it is cut short and
// reported as truncated by "byte-budget".
constexpr int32 McpMaxResultBytes = 24576; // matches DEFAULT_SEARCH_MAX_BYTES on the TS gateway (dogfood #3)

/** Typed error emitted when the generated capability catalog failed to load. */
TSharedPtr<FJsonObject> McpGatewayCatalogUnavailable(
	const FString& Operation, const FMcpCapabilityStore& Store);

/** Bounded, ranked, capability-level search over the generated catalog. */
TSharedPtr<FJsonObject> McpGatewaySearchCapabilities(
	const FMcpDiscoveryQuery& Query, const FMcpCapabilityStore& Store,
	FMcpToolEnabledPredicate IsToolEnabled);

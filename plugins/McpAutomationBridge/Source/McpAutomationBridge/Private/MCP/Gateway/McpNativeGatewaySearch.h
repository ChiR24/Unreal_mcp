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
	bool bHasDomain = false;
	bool bHasFamily = false;
	bool bHasAction = false;
	bool bHasParam = false;
	int32 Limit = 12;
	int32 Offset = 0;
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
constexpr int32 McpDescribeDefaultLimit = 20;
constexpr int32 McpDescribeMaxLimit = 50;
// Genuinely binding: the widest 25 results the catalog can produce total 10,263
// bytes, so a 16 KB cap could never fire. 8 KB bounds a full page of typical
// results and is exercised by real queries on both surfaces.
constexpr int32 McpMaxResultBytes = 8192;

/** Typed error emitted when the generated capability catalog failed to load. */
TSharedPtr<FJsonObject> McpGatewayCatalogUnavailable(
	const FString& Operation, const FMcpCapabilityStore& Store);

/** Bounded, ranked, capability-level search over the generated catalog. */
TSharedPtr<FJsonObject> McpGatewaySearchCapabilities(
	const FMcpDiscoveryQuery& Query, const FMcpCapabilityStore& Store,
	FMcpToolEnabledPredicate IsToolEnabled);

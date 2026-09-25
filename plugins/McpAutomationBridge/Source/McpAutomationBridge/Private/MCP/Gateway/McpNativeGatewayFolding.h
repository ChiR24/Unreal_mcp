// McpNativeGatewayFolding.h — folded capability families on the native door
//
// A folded record stands for a set of bridge actions that differ only by one
// selector value. Two data fields on the record drive the fold at execute time
// and nothing else does:
//   routing.dispatchBy    selector value -> bridge action, for a call that names
//                         the primary operation;
//   legacyIds[n].folded   the selector values an old name implied, for a call
//                         that still names the old action (or its alias id).
// Both doors apply the same two steps in the same order: the pins are injected
// before schema validation, the action is chosen after it. Mirror of
// src/server/gateway/gateway-dispatch-by.ts.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FMcpCapabilityRecord;
struct FMcpLegacyPair;

/** Parse legacyIds[] and routing.dispatchBy into the record. False with OutError on a malformed fold. */
bool McpParseRecordFolding(
	const TSharedPtr<FJsonObject>& Record, FMcpCapabilityRecord& Out, FString& OutError);

/** The record's pair named Action, primary or folded; null when it declares none. */
const FMcpLegacyPair* McpFindLegacyPair(const FMcpCapabilityRecord& Record, const FString& Action);

/**
 * The action the caller actually named: `action` of the generated form, or the
 * action segment of a `capability` that resolved through an alias. Empty when
 * the caller named the record by its own id.
 */
FString McpRequestedLegacyAction(const TSharedPtr<FJsonObject>& GatewayParams, const FString& RecordId);

/**
 * Inject the pins a folded old name implies, never overriding what the caller
 * sent. Returns false when the caller supplied the pinned selector with a
 * disagreeing value: a legacy caller never sent the selector pre-fold, so a
 * conflict is a contradictory request that must be refused, not dispatched.
 */
bool McpApplyFoldedPins(
	const FMcpCapabilityRecord& Record, const FString& RequestedAction,
	const TSharedPtr<FJsonObject>& Params);

/**
 * Fill in an omitted selector when the sent parameters that the variants do
 * not all share are declared by exactly one variant; otherwise leave the
 * params alone so the default variant runs as before.
 */
void McpInferFoldSelector(const FMcpCapabilityRecord& Record, const TSharedPtr<FJsonObject>& Params);

/**
 * The bridge action to dispatch once params are validated: an old name
 * dispatches itself, the primary maps its selector through dispatchBy, and
 * anything else is the primary action passed in. An unmapped selector value
 * returns EMPTY (fail closed) — validation enforces the selector's enum,
 * which the map keys equal, so this only guards a malformed record from
 * silently dispatching the primary.
 */
FString McpResolveDispatchAction(
	const FMcpCapabilityRecord& Record, const FString& RequestedAction,
	const TSharedPtr<FJsonObject>& Params, const FString& PrimaryAction);

/**
 * A consent grant naming a FOLDED {tool}.{action} pair authorizes that pair's
 * operation specifically. Returns false (with OutGrantedPairName set) when
 * such a grant is used to dispatch a different action of the same family; a
 * grant naming the canonical id, an alias, or a non-folded name authorizes
 * the whole family and always passes.
 */
bool McpFoldedGrantMatchesDispatch(
	const FMcpCapabilityRecord& Record, const FString& GrantedCapability,
	const FString& DispatchTarget, FString& OutGrantedPairName);

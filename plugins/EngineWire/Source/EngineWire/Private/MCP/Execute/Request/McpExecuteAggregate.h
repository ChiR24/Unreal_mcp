#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FMcpSemanticError;

// options.aggregate (McpExecuteAggregate.cpp): a list a reply would carry whole is counted, and grouped, in the plugin
// instead, so a caller can ask how many of what without the list entering its context.
namespace McpExecuteAggregate
{
/** options.aggregate is {field?: string, groupBy?: string, sample?: 0-10}; false (OutError set) when it is not. */
bool Validate(const TSharedPtr<FJsonObject>& Options, FMcpSemanticError& OutError);

/** options.aggregate as text for the idempotency fingerprint, so a replay asking another aggregate is not served the
 * first call's receipt; empty without one. */
FString FingerprintPart(const TSharedPtr<FJsonObject>& Options);

/**
 * Takes the aggregated array out of Result (the field named, else the largest array OutputSchema declares at the top,
 * else the largest at the top) and returns the receipt's aggregate block: field, total, and with groupBy the groups
 * (value, count, sample) most common first, capped at 50 (otherGroups counts the rest), or without it a sample of the
 * items. An item's sample is its handle (name, label, actorName, assetPath, path or id), else the item. A result with no
 * such array is left as it is and the block says why (note).
 */
TSharedPtr<FJsonObject> Apply(const TSharedPtr<FJsonObject>& Spec, const TSharedPtr<FJsonObject>& OutputSchema, TSharedPtr<FJsonObject>& InOutResult);
} // namespace McpExecuteAggregate

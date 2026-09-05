// McpNativeGatewaySearchMatch.h — word-level match rules for the native gateway search
//
// Mirrors `scoreRecord()` in tests/unit/plugin/gateway/native-discovery-search.ts
// exactly. Matching is WORD-level, never substring: "move" must not hit
// `remove_*`, and a namespace word must not make every record under it look
// like a hit. A record's declared aliases are its own names, so a verb the
// action does not carry ("move actor" -> control_actor.move_actor) still lands.
// Function words are dropped from the query, and regular plurals/inflections
// fold on both the query and the record text, so "list actors" meets "actor".

#pragma once

#include "CoreMinimal.h"

struct FMcpCapabilityRecord;

/** Lowercase ASCII alphanumeric runs of Text, folded, in order (`searchWords`). */
void McpSearchWords(const FString& Text, TArray<FString>& OutWords);

/** Query words minus closed-class function words, duplicates collapsed (first occurrence wins). */
void McpSearchContentWords(const TArray<FString>& AllWords, TArray<FString>& OutContent);

struct FMcpSearchMatch
{
	int32 Score = 0;
	/** Rules that fired, in fixed rule order regardless of which pass fired them. */
	TArray<FString> Reasons;
};

/**
 * Score one record against the lowercase trimmed Query, its full word list and
 * its content words. Returns false when no rule fired, in which case the record
 * is not a result at all.
 */
bool McpSearchScoreRecord(
	const FMcpCapabilityRecord& Record, const FString& Query,
	const TArray<FString>& AllWords, const TArray<FString>& ContentWords,
	FMcpSearchMatch& Out);

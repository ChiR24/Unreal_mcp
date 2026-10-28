#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpCapabilityStore;

// Typo correction for search (McpSearchTypos.cpp): a query word no record can match is replaced by the nearest word the
// catalog holds, so "spwan actor" ranks as "spawn actor". Mirrored by tests/eval/native-ranker.ts.
struct FMcpQueryCorrection
{
    FString From;
    FString To;
};

/**
 * LowerQuery with each word that no record's action words, aliases, topics, family, domain, parent or summary carry (so it
 * could score nothing) replaced by the nearest catalog word: Damerau-Levenshtein distance, a swap of two neighbours
 * counting one; at most 1 edit for words of 5-7 letters, 2 for 8 or more; shorter words, words with digits and function
 * words are left as they are, as is a word with no catalog word in reach. Ties go to the word more records carry, then
 * byte order. Each replacement is listed in OutCorrections.
 */
FString McpCorrectQuery(const FString& LowerQuery, const FMcpCapabilityStore& Store, TArray<FMcpQueryCorrection>& OutCorrections);

/** The search reply's corrected field, one {from, to} per word replaced; nothing when none was. */
void McpNoteCorrections(const TSharedPtr<FJsonObject>& Reply, const TArray<FMcpQueryCorrection>& Corrections);

/** Optimal string alignment distance (Damerau-Levenshtein without repeated edits of one substring), stopping early past
 * Limit: returns Limit + 1 once the distance must exceed it. */
int32 McpEditDistance(const FString& A, const FString& B, int32 Limit);

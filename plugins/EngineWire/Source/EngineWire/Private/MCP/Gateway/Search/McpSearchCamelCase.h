#pragma once

#include "CoreMinimal.h"

// CamelCase for search (McpSearchCamelCase.cpp), mirrored by tests/eval/native-ranker.ts. An engine name reads the same
// typed whole or spaced: the query is split at its CamelCase boundaries before it is lower-cased, and an engine name a
// topic or summary spells whole ('PlayerController') is matched by its parts through the query's joined spans.

/** Adjacent query words a joined span holds at most ("input mapping context" -> inputmappingcontext). */
constexpr int32 McpSearchJoinedSpanWords = 4;

/**
 * The parts of one ASCII alphanumeric run at its CamelCase boundaries: "GameplayEffect" -> Gameplay, Effect; "DSLParser"
 * -> DSL, Parser. A boundary is a lower-case letter before an upper-case one, unless the lower-case letters open the run
 * and no lower-case letter follows ("sRGB", "iOS", "macOS"); or the last letter of an upper-case run before a lower-case
 * one, unless that leaves one letter in front ("UObject", "AActor") or the lower-case one is a closing plural s ("LODs",
 * "NPCs"). A digit is never a boundary ("Texture2D", "K2Node").
 */
TArray<FString> McpSplitCamelCaseWord(const FString& Run);

/** Text with a space at each boundary McpSplitCamelCaseWord finds in each of its ASCII alphanumeric runs. */
FString McpSplitCamelCase(const FString& Text);

/** A run of adjacent query words written as one folded word, and the content words among them. */
struct FMcpJoinedSpan
{
    FString Word;
    TArray<FString> Covers;
};

/**
 * Every run of 2 to McpSearchJoinedSpanWords adjacent words of the lower-case Query, joined as typed and then folded
 * ("directories to scan" -> directoriestoscan, "sound cues" -> soundcue), with the content words it covers.
 */
TArray<FMcpJoinedSpan> McpSearchJoinedSpans(const FString& Query, const TArray<FString>& ContentWords);

/**
 * Whether Text holds Word itself, or the joined word of a span that covers it ('SoundCue' holds "sound" of "sound cue").
 * With bNamesOnly a span counts only where Text spells it as a name, opening with a capital ('PlayerController'), never
 * as a lower-camel field name ('currentLevel'): summary prose names its parameters and reply fields that way.
 */
bool McpSearchFieldHolds(const FString& Text, const FString& Word, const TArray<FMcpJoinedSpan>& Spans, bool bNamesOnly);

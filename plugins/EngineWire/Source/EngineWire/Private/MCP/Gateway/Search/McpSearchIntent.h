#pragma once

#include "CoreMinimal.h"

class FMcpCapabilityStore;
struct FMcpCapabilityRecord;

// What a search request asks for beyond its words (McpSearchIntent.cpp), read by McpSearchScoreRecord: the effect its
// first word opens, and whether a record ties the request's lead verb to the object asked about. Mirrored by
// tests/eval/native-ranker.ts, which reads the word lists from the .cpp.

/** The effect the query's first word, as typed, asks for: "read", "destructive" or empty. */
FString McpSearchIntendedEffect(const FString& LowerQuery);

/** Whether Word (folded) is the whole action of some record's id or alias: "move" (asset.move), "delete", "duplicate".
 * Such a word leading a request is the operation asked for. */
bool McpSearchIsWholeAction(const FString& Word, const FMcpCapabilityStore& Store);

/** Whether the record's action, one of its aliases or one of its topics holds the first content word together with
 * another content word: "move a vertex" ties move to vertex; "move a vertex" and "stack of crates" never tie move to crate. */
bool McpSearchLeadTied(const FMcpCapabilityRecord& Record, const TArray<FString>& ContentWords);

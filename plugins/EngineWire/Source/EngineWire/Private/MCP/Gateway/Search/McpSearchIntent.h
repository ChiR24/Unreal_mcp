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

/** The content words the record's action names. OutHeld: its own action or any alias holds the word, which counts it
 * as named and matched. OutScored: the ones the id rule scores, which are the words of its own action, those its domain
 * or family names as well, and those of the one alias that adds the most to them. Words spread over several other
 * aliases name no single action ("create", "material" and "color" from three members of a folded family). When the
 * request opens with an action verb (bLeadIsAction), an alias it does not ask for adds only its own verb: the request
 * asks for an alias when it holds one of its words that is neither generic, nor a function word, nor the verb of the
 * record's own action, or when the alias has no such word ("create actor with mesh component" does not ask for
 * create_spline_mesh_component, whose word apart is spline). */
void McpSearchActionWords(const FMcpCapabilityRecord& Record, const TArray<FString>& ContentWords, bool bLeadIsAction,
    const TSet<FString>& GenericWords, TArray<FString>& OutHeld, TArray<FString>& OutScored);

/** The words the actions (ids and aliases) of McpSearchGenericWordTools or more parent tools hold: too common to tell
 * one action from another. Built once from the catalog. */
const TSet<FString>& McpSearchGenericWords(const FMcpCapabilityStore& Store);

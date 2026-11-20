#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FMcpSemanticError;

// options.maxReplyChars (McpExecuteBudget.cpp): the caller's ceiling on one reply's size, for a client with a small
// context. Over it, the reply shrinks in a fixed order: its details and examples go, then its largest list goes out a
// page at a time (the gateway's own pager, so nothing is lost), then its longest strings are cut, each marked. Every cut
// is named in the reply's cuts. Measured on the result as the handler sent it, before select and omit.
namespace McpExecuteBudget
{
inline constexpr int32 MinChars = 1000;
inline constexpr int32 MaxChars = 100000;

/** A whole number of characters from MinChars to MaxChars; false (OutError set, INVALID_OPTIONS) when not. */
bool Validate(const TSharedPtr<FJsonObject>& Options, FMcpSemanticError& OutError);

/** The ceiling the options set, or 0 without one. */
int32 Of(const TSharedPtr<FJsonObject>& Options);

/** First rung: a copy of Payload without its top-level details and examples when it is over Budget (a field the
 *  output schema requires stays). Each one dropped is named in OutCuts. */
TSharedPtr<FJsonObject> DropExtras(const TSharedPtr<FJsonObject>& Payload, const TSharedPtr<FJsonObject>& OutputSchema,
                                   int32 Budget, TArray<FString>& OutCuts);

/** Last rung: Payload becomes a copy whose longest strings are cut, at 4000 characters, then 1000, then 200, until it
 *  fits; each cut string ends with the number of characters cut. True when it fits Budget. */
bool CutStrings(TSharedPtr<FJsonObject>& Payload, int32 Budget, TArray<FString>& OutCuts);
} // namespace McpExecuteBudget

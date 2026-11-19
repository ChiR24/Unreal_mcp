#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FMcpSemanticError;

// options.select / options.omit (McpExecuteSelect.cpp): the caller keeps or drops fields of the reply's data by dotted
// path, so it reads what it asked for and nothing else.
namespace McpExecuteSelect
{
/** Each is an array of 1-32 paths of 1-128 characters: dotted names, crossing lists (actors.label, or actors[].label);
 * false (OutError set, INVALID_OPTIONS) when one is not. */
bool Validate(const TSharedPtr<FJsonObject>& Options, FMcpSemanticError& OutError);

/** True when the options carry select or omit. */
bool Wants(const TSharedPtr<FJsonObject>& Options);

/** select and omit as text for the idempotency fingerprint; empty without them. */
FString FingerprintPart(const TSharedPtr<FJsonObject>& Options);

/**
 * A copy of Data keeping only the selected paths (several paths into one list merge per item), then without the
 * omitted ones; Data and everything it shares are left as they are. A path that matched nothing adds one warning.
 */
TSharedPtr<FJsonObject> Apply(const TSharedPtr<FJsonObject>& Options, const TSharedPtr<FJsonObject>& Data, TArray<FString>& OutWarnings);
} // namespace McpExecuteSelect

// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

struct FMcpSemanticError;

// execute's options.expect and options.require: what a call states about the editor after it runs and before it may
// run, judged by McpContracts. Each is an array of 1-16 assertions of at most 300 characters, each one parsed here so a
// malformed assertion is refused before anything is queued.
namespace McpExecuteContracts
{
bool Validate(const TSharedPtr<FJsonObject>& Options, FMcpSemanticError& OutError);

/** The assertions under Field ("expect" or "require"); empty when absent. */
TArray<FString> Read(const TSharedPtr<FJsonObject>& Options, const TCHAR* Field);

/** What the idempotency fingerprint adds for them, empty when there are none: a replay with other assertions is
 *  another call, never served the first one's receipt. */
FString FingerprintPart(const TSharedPtr<FJsonObject>& Options);
} // namespace McpExecuteContracts

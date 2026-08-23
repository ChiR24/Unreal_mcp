// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

namespace McpFabAddOperation
{
/** Composes the one page-side add script. ListingId must already pass IsSafeListingIdShared. */
FString BuildAddScript(const FString& RequestId, const FString& ListingId, const FString& EngineVersion);

/** "5.7" from the running engine, used to pick the matching listing version. */
FString CurrentEngineVersion();

/** True when Value is a safe Fab listing uid: non-empty, <=64 chars, [A-Za-z0-9_-] only. */
bool IsSafeListingIdShared(const FString& Value);
} // namespace McpFabAddOperation

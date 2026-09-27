// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

namespace McpFabAddOperation
{
/** Composes the one page-side add script. ListingId must already pass IsSafeListingId. */
FString BuildAddScript(const FString& RequestId, const FString& ListingId, const FString& EngineVersion);

/** True when Value is a safe Fab listing uid: non-empty, <=64 chars, [A-Za-z0-9_-] only. */
bool IsSafeListingId(const FString& Value);
} // namespace McpFabAddOperation

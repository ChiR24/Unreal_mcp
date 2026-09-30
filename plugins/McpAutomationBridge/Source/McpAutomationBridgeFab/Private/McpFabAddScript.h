// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

namespace McpFabAddOperation
{
/**
 * Composes the one page-side add script. ListingId must already pass IsSafeListingId. CombineMeshes
 * is what the caller said about merging meshes, unset when they said nothing.
 */
FString BuildAddScript(const FString& RequestId, const FString& ListingId, const FString& EngineVersion,
	const TOptional<bool>& CombineMeshes);

/** True when Value is a safe Fab listing uid: non-empty, <=64 chars, [A-Za-z0-9_-] only. */
bool IsSafeListingId(const FString& Value);
} // namespace McpFabAddOperation

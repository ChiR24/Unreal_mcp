// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

namespace McpFabAddOperation
{
/**
 * Composes the one page-side add script. ListingId must already pass IsSafeListingId. CombineMeshes
 * is what the caller said about merging meshes, unset when they said nothing. Quality is raw, high, mid
 * or low, or empty when the caller named none; it must already pass IsKnownQuality.
 */
FString BuildAddScript(const FString& RequestId, const FString& ListingId, const FString& EngineVersion,
	const TOptional<bool>& CombineMeshes, const FString& Quality);

/** True for the four quality tiers Megascans publishes, and for the empty string (no preference). */
bool IsKnownQuality(const FString& Value);

/** True when Value is a safe Fab listing uid: non-empty, <=64 chars, [A-Za-z0-9_-] only. */
bool IsSafeListingId(const FString& Value);
} // namespace McpFabAddOperation

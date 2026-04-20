// McpPinTypeParser.h — Bidirectional conversion between type DSL strings and FEdGraphPinType.
// Type DSL spec: docs/superpowers/specs/2026-04-20-bp-author-enum-struct-design.md §5

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "EdGraph/EdGraphPin.h"
#endif

class FMcpPinTypeParser
{
public:
#if WITH_EDITOR
	/**
	 * Parse a type DSL string into FEdGraphPinType.
	 * Returns true on success; on failure OutError describes the reason.
	 */
	static bool Parse(const FString& TypeString, FEdGraphPinType& OutPinType, FString& OutError);

	/**
	 * Serialize a FEdGraphPinType back to the canonical DSL string.
	 * Always produces a string; unknown shapes serialize to "Unknown<...>" with details
	 * in OutWarning so callers can detect lossy round-trips.
	 */
	static FString Serialize(const FEdGraphPinType& PinType, FString& OutWarning);
#endif
};

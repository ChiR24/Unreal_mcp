#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * Image-content helpers for tool results, split out of McpJsonRpc.cpp to keep
 * that translation unit under the 250 pure-line ceiling asserted by
 * tests/unit/plugin/source_structure_contracts.test.ts.
 */
namespace McpJsonRpcImage
{
/** Data with every imageBase64 replaced by a placeholder; returns Data untouched when there is none. */
TSharedPtr<FJsonObject> MakeToolTextData(const TSharedPtr<FJsonObject>& Data);

/** Appends an MCP image content block when Data carries an imageBase64 at any depth. */
void AddImageContentIfPresent(const TSharedPtr<FJsonObject>& Data,
	TArray<TSharedPtr<FJsonValue>>& Content);
}

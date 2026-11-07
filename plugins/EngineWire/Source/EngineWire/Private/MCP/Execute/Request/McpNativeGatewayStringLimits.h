// McpNativeGatewayStringLimits.h — the input-only string length gate of execute
//
// An FName holds at most NAME_SIZE - 1 (1023) characters: one more asserts in the engine ("FName's 1023 max length
// exceeded", UnrealNames.cpp) and closes the editor, and hundreds of handlers build an FName from a parameter. So
// execute holds every string a call sends to its schema's maxLength, or to McpDefaultStringMaxLength where the schema
// sets none or does not reach (an untyped value, a reflection-boundary interior), and every object key to the default.
// A record raises a parameter that carries long text (code, a script, prose) with its own maxLength. Replies are
// never held to it: this runs on the params of execute only, beside the canonical schema validation.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "MCP/Execute/McpNativeGatewaySchemaValidation.h"

inline constexpr int32 McpDefaultStringMaxLength = NAME_SIZE - 1;

/** False with a Range violation naming the string or the object holding the key; also past the pre-queue gate's nesting bound. */
bool McpCheckInputStringLengths(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonObject>& Schema, FMcpSchemaViolationDetail& OutViolation);

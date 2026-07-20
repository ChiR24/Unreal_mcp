// McpNativeGatewaySchemaKeywords.h — per-keyword semantics for the canonical schema subset
//
// One keyword, one question: is this keyword implemented, does this value match
// a declared type, are two values equal, is a bound satisfied. The traversal that
// walks a schema document and applies these lives in McpNativeGatewaySchemaValidation.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "MCP/Execute/McpNativeGatewaySchemaValidation.h"

namespace McpSchemaKeywords
{
/** False for any keyword the canonical validator does not implement (fail-closed). */
bool IsSupportedKeyword(const FString& Keyword);

FString JoinPointer(const FString& Pointer, const FString& Segment);
FString PointerOrRoot(const FString& Pointer);

bool ValueMatchesType(const TSharedPtr<FJsonValue>& Value, const FString& Declared);
TArray<FString> DeclaredTypes(const TSharedPtr<FJsonObject>& Schema);
bool JsonValuesEqual(const TSharedPtr<FJsonValue>& Left, const TSharedPtr<FJsonValue>& Right);

FMcpSchemaViolationDetail MakeViolation(
	EMcpSchemaViolation Reason, const FString& Pointer, const FString& Message);

/** Numeric range, string length, array item-count and uniqueness bounds. */
bool CheckBounds(
	const TSharedPtr<FJsonValue>& Value, const TSharedPtr<FJsonObject>& Schema,
	const FString& Pointer, FMcpSchemaViolationDetail& OutViolation);
}

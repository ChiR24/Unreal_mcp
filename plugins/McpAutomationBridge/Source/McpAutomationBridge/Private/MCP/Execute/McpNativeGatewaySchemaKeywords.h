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

/** At-least-one enforcement for the requiredOneOf keyword. */
bool CheckRequiredOneOf(
	const TSharedPtr<FJsonObject>& Object, const TSharedPtr<FJsonObject>& Schema,
	const FString& Pointer, FMcpSchemaViolationDetail& OutViolation);

FString DescribeAllowedValues(const TArray<TSharedPtr<FJsonValue>>& Allowed, int32 MaxNames);

/** Names what WOULD work: nearest declared spelling plus the declared key list. */
FString DescribeUndeclaredParameter(
	const FString& Key, const TSharedPtr<FJsonObject>& Properties);

/**
 * A missing ENUM parameter is the one case the caller cannot guess, so the
 * refusal lists what it accepts. Matches the TypeScript gateway's
 * describeMissingParameter exactly.
 */
inline FString DescribeMissingParameter(
	const FString& Name, const TSharedPtr<FJsonObject>& Properties)
{
	const TSharedPtr<FJsonObject>* PropertySchema = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Allowed = nullptr;
	if (Properties.IsValid() &&
		Properties->TryGetObjectField(Name, PropertySchema) && PropertySchema &&
		(*PropertySchema)->TryGetArrayField(TEXT("enum"), Allowed) && Allowed)
	{
		TArray<FString> Names;
		for (const TSharedPtr<FJsonValue>& Entry : *Allowed)
		{
			FString AsString;
			if (Entry.IsValid() && Entry->TryGetString(AsString)) { Names.Add(AsString); }
		}
		if (Names.Num() > 0)
		{
			return FString::Printf(TEXT("Missing required parameter '%s' (one of: %s)"),
				*Name, *FString::Join(Names, TEXT(", ")));
		}
	}
	return FString::Printf(TEXT("Missing required parameter '%s'"), *Name);
}
}

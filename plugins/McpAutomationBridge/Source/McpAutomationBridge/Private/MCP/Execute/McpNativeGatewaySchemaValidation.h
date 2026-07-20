// McpNativeGatewaySchemaValidation.h — exact per-action schema enforcement
//
// Implements the Draft-2020-12 keyword subset the canonical capability records
// actually use, and nothing else. The matching TypeScript specification is
// tests/unit/task-27-suite/schema-subset.ts; both surfaces must produce the same
// violation reason for the same input.
//
// Unsupported keywords fail CLOSED. If a record ever grows a keyword this
// validator does not implement (a conditional, $ref, allOf/anyOf/oneOf, pattern,
// format), execute rejects the call instead of silently under-validating it.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

enum class EMcpSchemaViolation : uint8
{
	None,
	MissingRequired,
	Undeclared,
	Type,
	Enum,
	Range,
	UnsupportedKeyword,
};

struct FMcpSchemaViolationDetail
{
	EMcpSchemaViolation Reason = EMcpSchemaViolation::None;
	FString Pointer;
	FString Message;
};

/** Stable gateway error code for a violation reason, shared with TypeScript. */
const TCHAR* McpSchemaViolationCode(EMcpSchemaViolation Reason);

/** Semantic error kind for a violation reason ("validation" or "range"). */
const TCHAR* McpSchemaViolationKind(EMcpSchemaViolation Reason);

/** True when the value satisfies the schema. OutViolation is set otherwise. */
bool McpValidateAgainstCanonicalSchema(
	const TSharedPtr<FJsonValue>& Value, const TSharedPtr<FJsonObject>& Schema,
	FMcpSchemaViolationDetail& OutViolation);

/** Object convenience overload used by the execute path for params and results. */
bool McpValidateObjectAgainstCanonicalSchema(
	const TSharedPtr<FJsonObject>& Object, const TSharedPtr<FJsonObject>& Schema,
	FMcpSchemaViolationDetail& OutViolation);

/** Apply declared property defaults before validation, mirroring TypeScript. */
TSharedPtr<FJsonObject> McpApplyCanonicalSchemaDefaults(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonObject>& Schema);

/** True when the schema declares a property of this name. */
bool McpSchemaDeclaresProperty(const TSharedPtr<FJsonObject>& Schema, const TCHAR* PropertyName);

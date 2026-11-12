// McpNativeGatewaySchemaValidation.h — exact per-action schema enforcement
//
// Implements the Draft-2020-12 keyword subset the canonical capability records
// actually use, and nothing else.
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
	RequiredOneOf,
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

/** Stable gateway error code for a violation reason. */
const TCHAR* McpSchemaViolationCode(EMcpSchemaViolation Reason);

/** True when the value satisfies the schema. OutViolation is set otherwise. */
bool McpValidateAgainstCanonicalSchema(
	const TSharedPtr<FJsonValue>& Value, const TSharedPtr<FJsonObject>& Schema,
	FMcpSchemaViolationDetail& OutViolation);

/** Object convenience overload used by the execute path for params and results. */
bool McpValidateObjectAgainstCanonicalSchema(
	const TSharedPtr<FJsonObject>& Object, const TSharedPtr<FJsonObject>& Schema,
	FMcpSchemaViolationDetail& OutViolation);

/** Converts {x,y,z}/{width,height}/{r,g,b,a} objects to arrays and back to match the declared shape. */
TSharedPtr<FJsonObject> McpCoerceCanonicalVectorShapes(const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonObject>& Schema);
/** Apply declared property defaults before validation. */
TSharedPtr<FJsonObject> McpApplyCanonicalSchemaDefaults(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonObject>& Schema);

/** True when the schema declares a property of this name. */
bool McpSchemaDeclaresProperty(const TSharedPtr<FJsonObject>& Schema, const TCHAR* PropertyName);

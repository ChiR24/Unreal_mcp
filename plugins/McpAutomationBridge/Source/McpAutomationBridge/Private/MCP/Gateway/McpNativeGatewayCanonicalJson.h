// McpNativeGatewayCanonicalJson.h — deterministic, cross-language JSON rendering
//
// Discovery responses are compared byte-for-byte against the TypeScript gateway
// reference, so both surfaces must serialize identically. The rules are:
//   * object keys sorted by code unit, compact separators, no whitespace
//   * every code unit above 0x7e escaped as \uXXXX, so output is pure ASCII
//   * integers printed as integers; a non-integer number is refused, because
//     shortest-round-trip float text is the one place C++ and JavaScript can
//     legitimately disagree. Record subtrees carrying floats (examples) are
//     represented by their generated content hash instead of being inlined.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/** Render a JSON value canonically. Returns false if a non-integer number is reached. */
bool McpCanonicalJson(const TSharedPtr<FJsonValue>& Value, FString& OutJson);

/** Render a JSON object canonically. Returns false if a non-integer number is reached. */
bool McpCanonicalJsonObject(const TSharedPtr<FJsonObject>& Object, FString& OutJson);

/** UTF-8 byte length of an already-rendered canonical string (ASCII-only, so 1 byte per char). */
int32 McpCanonicalByteLength(const FString& CanonicalJson);

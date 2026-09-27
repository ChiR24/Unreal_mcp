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

/**
 * Render a JSON object canonically; the output is ASCII, so Len() is its UTF-8 byte
 * length. A non-integer number fails the render unless bAllowFractions, which an
 * in-process consumer (the idempotency fingerprint) sets: it prints SanitizeFloat.
 */
bool McpCanonicalJsonObject(const TSharedPtr<FJsonObject>& Object, FString& OutJson, bool bAllowFractions = false);

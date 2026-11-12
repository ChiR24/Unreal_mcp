// McpSecretMask.h — secret masking for replies, receipts and logs
//
// Masks secret-looking token/secret/password/api_key/authorization/Bearer values
// (including `Authorization: Bearer <token>` and JSON-like `"token":"x"`) so a
// credential echoed by a handler never survives serialization. The response
// funnel (Core) and the native receipts (MCP/Execute) both mask through here.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** The deepest a reply's JSON tree is walked (masking, the image search). The size gate cannot substitute for a
 *  depth gate: thousands of nesting levels are only a few KB, so the stack overflows before any size check runs. */
inline constexpr int32 McpMaxReplyTreeDepth = 100;

/** Mask secret-looking assignments and Bearer tokens in free text. */
FString McpMaskSecrets(const FString& Value);

/** True when an object KEY names a credential. Defined in McpSecretKeys.cpp.
 *  Value-only masking cannot see a credential that arrives as a real JSON value,
 *  because no `keyword<sep>value` context survives — the key name is the only signal. */
bool McpIsSecretKey(const FString& Key);

/** True when Key is a generic value carrier — a key that names no subject of its
 *  own and takes its meaning from a NAME-BEARING sibling in the same object.
 *  Defined in McpSecretKeys.cpp. */
bool McpIsGenericValueKey(const FString& Key);

/** True when this object names a credential in a NAME-BEARING field while carrying
 *  the credential itself in a generic sibling — the reflection reply shape
 *  `{propertyName: "CapabilityToken", value: "<token>"}`. Key-name classification
 *  alone cannot see that: the key holding the secret is `value`, which names
 *  nothing, and the name that would betray it sits in a different field. Defined
 *  in McpSecretKeys.cpp. */
bool McpNamesCredentialBySibling(const TSharedPtr<FJsonObject>& Object);

/** Recursively mask secret string leaves in a JSON object tree in place, and
 *  mask a secret-NAMED key's entire value whatever its shape. Bounded by a depth
 *  cap so a deeply nested tree cannot overflow the stack. */
void McpMaskSecretsDeep(const TSharedPtr<FJsonObject>& Object);

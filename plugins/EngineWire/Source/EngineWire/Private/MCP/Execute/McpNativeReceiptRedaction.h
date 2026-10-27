// McpNativeReceiptRedaction.h — secret masking + receipt bounds
//
// Native mirror of src/tools/catalog/capabilities/semantic/receipt-redaction.ts:
// masks secret-looking token/secret/password/api_key/authorization/Bearer values
// (including `Authorization: Bearer <token>` and JSON-like `"token":"x"`) so a
// credential echoed by a handler never survives serialization, caps receipt
// arrays and free text, and bounds an oversized handler result like the TS gateway.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** Mask secret-looking assignments and Bearer tokens in free text. */
FString McpMaskSecrets(const FString& Value);

/** Mask secrets then truncate to the receipt free-text bound, mirroring TS redactText. */
FString McpRedactText(const FString& Value);

/** True when an object KEY names a credential, mirroring isSecretKey() in
 *  receipt-redaction.ts. Defined in McpNativeReceiptSecretKeys.cpp. Value-only
 *  masking cannot see a credential that arrives as a real JSON value, because
 *  no `keyword<sep>value` context survives — the key name is the only signal. */
bool McpIsSecretKey(const FString& Key);

/** True when Key is a generic value carrier — a key that names no subject of its
 *  own and takes its meaning from a NAME-BEARING sibling in the same object.
 *  Defined in McpNativeReceiptSecretKeys.cpp; mirrors GENERIC_VALUE_KEYS in
 *  receipt-redaction.ts. */
bool McpIsGenericValueKey(const FString& Key);

/** True when this object names a credential in a NAME-BEARING field while carrying
 *  the credential itself in a generic sibling — the reflection reply shape
 *  `{propertyName: "CapabilityToken", value: "<token>"}`. Key-name classification
 *  alone cannot see that: the key holding the secret is `value`, which names
 *  nothing, and the name that would betray it sits in a different field. Defined
 *  in McpNativeReceiptSecretKeys.cpp; mirrors namesCredentialBySibling() in
 *  receipt-redaction.ts. */
bool McpNamesCredentialBySibling(const TSharedPtr<FJsonObject>& Object);

/** Recursively mask secret string leaves in a JSON object tree in place, and
 *  mask a secret-NAMED key's entire value whatever its shape. Bounded by the
 *  shared receipt depth cap so a deeply nested tree cannot overflow the stack. */
void McpMaskSecretsDeep(const TSharedPtr<FJsonObject>& Object);

/** Cap a receipt array to the shared max entry count, mirroring TS boundArray. */
TArray<TSharedPtr<FJsonValue>> McpBoundJsonArray(TArray<TSharedPtr<FJsonValue>> Values);

/** True when the condensed JSON serialization of Result exceeds MaxChars, matching
 *  the TS gateway's serialized result-size limit. */
bool McpSerializedResultExceeds(
	const TSharedPtr<FJsonObject>& Result, int32 MaxChars, int64* OutSerializedChars = nullptr);

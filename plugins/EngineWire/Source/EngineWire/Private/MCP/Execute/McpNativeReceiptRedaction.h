// McpNativeReceiptRedaction.h — receipt bounds
//
// Caps receipt arrays and free text, and bounds an oversized handler result. The
// masking they build on is Foundation's (McpSecretMask.h), included for callers.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/Security/McpSecretMask.h"

/** Mask secrets then truncate to the receipt free-text bound. */
FString McpRedactText(const FString& Value);

/** Cap a receipt array to the shared max entry count. */
TArray<TSharedPtr<FJsonValue>> McpBoundJsonArray(TArray<TSharedPtr<FJsonValue>> Values);

/** True when the condensed JSON serialization of Result exceeds MaxChars, the
 *  serialized result-size limit. */
bool McpSerializedResultExceeds(
	const TSharedPtr<FJsonObject>& Result, int32 MaxChars, int64* OutSerializedChars = nullptr);

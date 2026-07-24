// McpNativeReceiptEnrichment.h — canonical receipt assembly + secret masking
//
// Builds the nested canonical `receipt` object (mirroring the TypeScript
// ReceiptSchema after normalizers) that both the success and error native
// envelopes carry, and provides the secret-masking + request-id canonicalization
// primitives shared across the native receipt path.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

struct FMcpSemanticError;
struct FMcpReceiptContext;

/** Canonicalize a JSON-RPC id to the collision-free num:/str: form the TS receipt
 *  uses. Never the internal automation id. */
FString McpCanonicalizeRequestId(const TSharedPtr<FJsonValue>& Id);

/** Nested canonical receipt shared by the success and error native envelopes. On
 *  success, Data is the projected canonical output and RawResult is mined for
 *  reusable handles/changes/task; on error the typed algebra is the `error`
 *  object. Timing/ids/revisions come from Context and the resolved record. */
TSharedPtr<FJsonObject> McpBuildCanonicalReceipt(
	const FString& CapabilityId, const FMcpReceiptContext& Context,
	bool bSuccess, const FMcpSemanticError* Error,
	const TSharedPtr<FJsonObject>& Data, const TSharedPtr<FJsonObject>& RawResult);

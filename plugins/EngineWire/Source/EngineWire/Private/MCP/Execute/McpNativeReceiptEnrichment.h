// McpNativeReceiptEnrichment.h — canonical receipt assembly + secret masking
//
// Builds the nested canonical `receipt` object that both the success and error native
// envelopes carry, and provides the secret-masking + request-id canonicalization
// primitives shared across the native receipt path.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

struct FMcpSemanticError;
struct FMcpReceiptContext;

// capabilityRevision / schemaRevision on a receipt come straight from the resolved
// record's content/schema hashes, so all three revision strings stay distinct and truthful.
void McpSetReceiptRecordRevisions(const TSharedPtr<FJsonObject>& Receipt, const FString& CapabilityId);

/** Canonicalize a JSON-RPC id to the collision-free num:/str: form the receipt
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

// McpNativeGatewayReceipt.h — semantic receipt and typed error envelope
//
// Mirrors the TypeScript contracts in
// src/tools/catalog/capabilities/semantic/{envelope,errors}.ts: a
// receipt is discriminated by `status`, always names its capability, and an
// error carries the typed algebra (`kind` + `code`) rather than a bare string.
//
// The pre-Task-27 gateway guidance fields (errorCode, suggestions, nextCall) are
// retained alongside the typed error so existing clients keep their guided
// recovery path, and a failed Unreal dispatch keeps its structured detail under
// `unrealDetail` instead of being flattened into a message.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/McpLiveStateRevisions.h"

struct FMcpSemanticError
{
	FString Kind;
	FString Code;
	FString GatewayCode;
	FString Message;
	FString Pointer;
	FString Option;
	FString Field;
	// Unreal's own error code when the handler refused the call, mirroring the
	// TypeScript execution variant's `handlerCode`: the fixed algebra label stays
	// on Code, this names the specific reason.
	FString HandlerCode;
	// A real boolean carried on the typed error (kinds capability,
	// dispatch and execution), replacing the earlier Field="retryable" string
	// hack so retryability is a true boolean matching the TypeScript algebra.
	bool bHasRetryable = false;
	bool bRetryable = false;
	// Stale-revision references, set only on a staleState error.
	FString CurrentRevision;
	FString ExpectedRevision;
	// Security-policy payload, each set only on its own kind:
	// authorization (requiredScope + grantedScopes) and consent (scope).
	FString RequiredScope;
	TArray<FString> GrantedScopes;
	FString ConsentScope;
	TArray<FString> Supported;
	// Set only on RESULT_TOO_LARGE, mirroring the TypeScript output variant so a
	// client learns by how much a response overran on either transport.
	bool bHasResultChars = false;
	int64 ResultChars = 0;
	TSharedPtr<FJsonObject> UnrealDetail;
};

/** Who may read on through a result too large for one reply: the principal, capability and params of the call that
 *  produced it (McpNativeResultPages.cpp; the TypeScript twin is gateway-result-pages.ts). */
struct FMcpPageOwner
{
	FString Principal;
	FString CapabilityId;
	FString Fingerprint;
	/** The call's own routing (capability, or tool and action) and params, for the page block's continuation call. */
	TSharedPtr<FJsonObject> Call;
};

/** One page of such a result: the result with its array cut to what fits, and the page block (field, offset,
 *  returned, total; cursor and nextCall while items remain). */
struct FMcpResultPage
{
	TSharedPtr<FJsonObject> Result;
	TSharedPtr<FJsonObject> Page;
};

/** The first page of an oversized result; false when it cannot be paged (no top-level array of two or more items,
 *  the rest alone too large, or one item too large for a reply of its own). */
bool McpFirstResultPage(const TSharedPtr<FJsonObject>& Result, int64 Budget, const FMcpPageOwner& Owner, FMcpResultPage& Out);

/** The next page for a cursor, or why there is none: PAGE_CURSOR_EXPIRED (expired, read to the end or never issued)
 *  or PAGE_CURSOR_MISMATCH (another principal, capability or params). */
FString McpNextResultPage(const FString& Cursor, const FMcpPageOwner& Owner, int64 Budget, FMcpResultPage& Out);

// Per-request join keys + timing threaded onto every receipt so success and error
// envelopes carry the same correlation/request/idempotency ids and timing as the
// TypeScript receipt. RequestId is the canonicalized external MCP id (num:/str:),
// never the internal automation id; IdempotencyId is the client's
// options.idempotencyKey; StartTimeSeconds is FPlatformTime::Seconds() captured
// when the request began (0 => timing omitted).
struct FMcpReceiptContext
{
	FString CorrelationId;
	FString RequestId;

	/** The id the subsystem queue and SendAutomationResponse use for this call.
	 *  RequestId above is the CLIENT's JSON-RPC id ("num:8") and is what every
	 *  receipt reports; the queue keys on a GUID minted per dispatch. Anything
	 *  that has to pair a pre-dispatch decision with the response -- the consent
	 *  burn ledger does -- must key on THIS one or it will never find its entry.
	 */
	FString QueueRequestId;
	FString IdempotencyId;
	FString IdempotencySlot;
	double StartTimeSeconds = 0.0;

	/** Live-state pins, carried to the game-thread gate that enforces them. */
	TMap<EMcpStateKind, int64> ExpectedRevisions;

	/** Receipt warnings the gateway found before dispatch (McpUnreadVariantParams). */
	TArray<FString> GatewayWarnings;

	/** resolvedFromAlias / migratedFrom of the request, copied onto every receipt (FMcpGatewayExecuteRequest). */
	TSharedPtr<FJsonObject> Provenance;

	/** Who may page through this call's result when it is too large for one reply; unset, it is refused instead. */
	TSharedPtr<FMcpPageOwner> PageOwner;
};

FMcpSemanticError McpValidationError(const FString& GatewayCode, const FString& Message, const FString& Pointer = FString());
FMcpSemanticError McpOptionError(const FString& Option, const TArray<FString>& Supported, const FString& Message);
FMcpSemanticError McpRangeError(const FString& Field, const FString& Message);
FMcpSemanticError McpExecutionError(const FString& GatewayCode, const FString& Message, bool bRetryable);
FMcpSemanticError McpUnrealExecutionError(const FString& Message, const TSharedPtr<FJsonObject>& UnrealDetail);
// Plan-class constructors mirroring the TypeScript typed error algebra.
FMcpSemanticError McpCapabilityError(const FString& GatewayCode, const FString& Code, const FString& Message, bool bRetryable);
FMcpSemanticError McpDispatchError(const FString& GatewayCode, const FString& Code, const FString& Message, bool bRetryable);
FMcpSemanticError McpOutputError(const FString& Code, const FString& Message, const FString& Pointer = FString());
FMcpSemanticError McpStaleStateError(const FString& Message, const FString& CurrentRevision, const FString& ExpectedRevision);

/** Order-independent SHA-256 digest of the post-normalization params;
 *  the ledger conflicts a key replayed with different effective arguments. */
FString McpCanonicalFingerprint(const FString& CapabilityId, const TSharedPtr<FJsonObject>& Params);

/** Settle one ledger slot from the completed receipt at the single
 *  completion funnel — success is recorded for replay, anything else releases
 *  the slot so the key stays retryable. A blank slot is a no-op. */
void McpSettleIdempotency(const FString& Slot, bool bSuccess, const TSharedPtr<FJsonObject>& Receipt);

/** Error receipt. Guidance (suggestions/nextCall/etc.) is merged in when supplied. */
TSharedPtr<FJsonObject> McpBuildErrorReceipt(
	const FString& CapabilityId, const FMcpSemanticError& Error,
	const FMcpReceiptContext& Context, const TSharedPtr<FJsonObject>& Guidance = nullptr);

/** Success receipt carrying the validated handler payload as `data`. The raw
 *  handler result (when supplied) is mined for reusable handles/changes/task. */
TSharedPtr<FJsonObject> McpBuildSuccessReceipt(
	const FString& CapabilityId, const TSharedPtr<FJsonObject>& Data,
	const FMcpReceiptContext& Context, const TSharedPtr<FJsonObject>& RawResult = nullptr,
	const FString& Message = FString());

/** Human-readable one-line summary used as the MCP text content. */
FString McpReceiptMessage(const TSharedPtr<FJsonObject>& Receipt);

/** True when the receipt reports `status: "success"`. */
bool McpReceiptSucceeded(const TSharedPtr<FJsonObject>& Receipt);

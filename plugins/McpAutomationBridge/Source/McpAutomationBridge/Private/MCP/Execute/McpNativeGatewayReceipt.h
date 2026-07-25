// McpNativeGatewayReceipt.h — semantic receipt and typed error envelope
//
// Mirrors the TypeScript contracts in
// src/tools/catalog/capabilities/semantic/{envelope,errors}.ts (Task 3): a
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

struct FMcpSemanticError
{
	FString Kind;
	FString Code;
	FString GatewayCode;
	FString Message;
	FString Pointer;
	FString Option;
	FString Field;
	// Task 39: a real boolean carried on the typed error (kinds capability,
	// dispatch and execution), replacing the earlier Field="retryable" string
	// hack so retryability is a true boolean matching the TypeScript algebra.
	bool bHasRetryable = false;
	bool bRetryable = false;
	// Task 39: stale-revision references, set only on a staleState error.
	FString CurrentRevision;
	FString ExpectedRevision;
	// Task 40 security-policy payload, each set only on its own kind:
	// authorization (requiredScope + grantedScopes) and consent (scope).
	FString RequiredScope;
	TArray<FString> GrantedScopes;
	FString ConsentScope;
	TArray<FString> Supported;
	TSharedPtr<FJsonObject> UnrealDetail;
};

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
	FString IdempotencyId;
	double StartTimeSeconds = 0.0;
};

FMcpSemanticError McpValidationError(const FString& GatewayCode, const FString& Message, const FString& Pointer = FString());
FMcpSemanticError McpOptionError(const FString& Option, const TArray<FString>& Supported, const FString& Message);
FMcpSemanticError McpRangeError(const FString& Field, const FString& Message);
FMcpSemanticError McpExecutionError(const FString& GatewayCode, const FString& Message, bool bRetryable);
FMcpSemanticError McpUnrealExecutionError(const FString& Message, const TSharedPtr<FJsonObject>& UnrealDetail);
// Task 39 plan-class constructors mirroring the TypeScript typed error algebra.
FMcpSemanticError McpCapabilityError(const FString& GatewayCode, const FString& Code, const FString& Message, bool bRetryable);
FMcpSemanticError McpDispatchError(const FString& GatewayCode, const FString& Code, const FString& Message, bool bRetryable);
FMcpSemanticError McpOutputError(const FString& Code, const FString& Message, const FString& Pointer = FString());
FMcpSemanticError McpStaleStateError(const FString& Message, const FString& CurrentRevision, const FString& ExpectedRevision);

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

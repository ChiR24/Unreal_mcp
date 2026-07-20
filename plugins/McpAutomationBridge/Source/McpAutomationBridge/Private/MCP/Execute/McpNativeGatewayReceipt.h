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
	TArray<FString> Supported;
	TSharedPtr<FJsonObject> UnrealDetail;
};

FMcpSemanticError McpValidationError(const FString& GatewayCode, const FString& Message, const FString& Pointer = FString());
FMcpSemanticError McpOptionError(const FString& Option, const TArray<FString>& Supported, const FString& Message);
FMcpSemanticError McpRangeError(const FString& Field, const FString& Message);
FMcpSemanticError McpExecutionError(const FString& GatewayCode, const FString& Message, bool bRetryable);
FMcpSemanticError McpUnrealExecutionError(const FString& Message, const TSharedPtr<FJsonObject>& UnrealDetail);

/** Error receipt. Guidance (suggestions/nextCall/etc.) is merged in when supplied. */
TSharedPtr<FJsonObject> McpBuildErrorReceipt(
	const FString& CapabilityId, const FMcpSemanticError& Error,
	const FString& CorrelationId, const TSharedPtr<FJsonObject>& Guidance = nullptr);

/** Success receipt carrying the validated handler payload as `data`. */
TSharedPtr<FJsonObject> McpBuildSuccessReceipt(
	const FString& CapabilityId, const TSharedPtr<FJsonObject>& Data,
	const FString& CorrelationId, const FString& Message = FString());

/** Human-readable one-line summary used as the MCP text content. */
FString McpReceiptMessage(const TSharedPtr<FJsonObject>& Receipt);

/** True when the receipt reports `status: "success"`. */
bool McpReceiptSucceeded(const TSharedPtr<FJsonObject>& Receipt);

#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"

#include "Containers/StringConv.h"
#include "Dom/JsonValue.h"
#include "Foundation/McpIdempotencyLedger.h"
#include "MCP/Gateway/McpNativeGatewayCanonicalJson.h"
#include "MCP/Execute/McpNativeGatewayExecuteRequest.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Foundation/McpSecureTokenCompare.h"

FString McpCanonicalFingerprint(const FString& CapabilityId, const TSharedPtr<FJsonObject>& Params)
{
	// Sorted keys make the digest order-independent: payloads differing only in
	// key order share a fingerprint. A null Params renders as {}.
	FString Rendered;
	// A render that stopped part way is a prefix other payloads share; a fresh GUID
	// fails closed (a retry reads as a conflict, never as someone else's replay).
	if (!McpCanonicalJsonObject(Params, Rendered, /*bAllowFractions=*/true))
	{
		Rendered = TEXT("!unrenderable:") + FGuid::NewGuid().ToString();
	}
	const FString Canonical = CapabilityId + TEXT(" ") + Rendered;

	return McpSha256Hex(Canonical);
}

bool McpValidateIdempotencyKeyOption(
	const TSharedPtr<FJsonObject>& Options, FMcpSemanticError& OutError)
{
	if (!Options.IsValid())
	{
		return true;
	}
	const TSharedPtr<FJsonValue> Key = Options->TryGetField(TEXT("idempotencyKey"));
	if (!Key.IsValid())
	{
		return true;
	}
	FString Value;
	const bool bBoundedString = Key->Type == EJson::String && McpHandlerUtils::TryGetJsonValueString(Key, Value) &&
		Value.Len() >= 1 && Value.Len() <= 128;
	if (bBoundedString)
	{
		return true;
	}
	OutError = McpValidationError(TEXT("INVALID_OPTIONS"),
		TEXT("options.idempotencyKey must be a string of 1..128 characters."),
		TEXT("/options/idempotencyKey"));
	OutError.Option = TEXT("idempotencyKey");
	return false;
}

void McpSettleIdempotency(const FString& Slot, bool bSuccess, const TSharedPtr<FJsonObject>& Receipt)
{
	if (Slot.IsEmpty())
	{
		return;
	}
	FMcpIdempotencyLedger& Ledger = FMcpIdempotencyLedger::Get();
	if (!bSuccess || !Receipt.IsValid())
	{
		Ledger.Abandon(Slot);
		return;
	}
	FString ReceiptJson;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&ReceiptJson);
	FJsonSerializer::Serialize(Receipt.ToSharedRef(), Writer);
	Ledger.Complete(Slot, ReceiptJson);
}

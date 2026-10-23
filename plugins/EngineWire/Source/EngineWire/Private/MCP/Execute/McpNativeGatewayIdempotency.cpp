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
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"

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

namespace McpNativeLoopGuard
{
namespace
{
constexpr int32 LoopLimit = 3;
constexpr double LoopWindowSeconds = 60.0;

struct FStreak
{
	FString Fingerprint;
	FString Code;
	int32 Count = 0;
	double At = 0.0;
};

// ponytail: one streak per principal identity with no eviction; identities are loopback, legacy and the configured
// scoped profiles, so the map stays tiny. Two sessions of one principal share a streak.
FCriticalSection GMutex;
TMap<FString, FStreak> GStreaks;

// Mirrors MOMENT_CODE in gateway-loop-guard.ts: the same call can succeed later, or with options or consent changed.
bool IsMomentCode(const FString& Code)
{
	static const TSet<FString> Moments = {TEXT("EDITOR_MODAL_ACTIVE"), TEXT("EDITOR_BLOCKED"), TEXT("EDITOR_TOOL_BLOCKED"),
		TEXT("STALE_STATE"), TEXT("QUOTA_EXCEEDED"), TEXT("NOT_CONNECTED"), TEXT("CANCELLED"), TEXT("MCP_REQUEST_CANCELLED")};
	return (Code.EndsWith(TEXT("TIMEOUT"), ESearchCase::CaseSensitive) && !Code.StartsWith(TEXT("INVALID_"), ESearchCase::CaseSensitive)) ||
		Code.EndsWith(TEXT("_BUSY"), ESearchCase::CaseSensitive) || Code.StartsWith(TEXT("CONSENT_"), ESearchCase::CaseSensitive) ||
		Moments.Contains(Code);
}
} // namespace
} // namespace McpNativeLoopGuard

FString McpLoopGuardRefusal(const TSharedPtr<FMcpPageOwner>& Owner)
{
	using namespace McpNativeLoopGuard;
	if (!Owner.IsValid())
	{
		return FString();
	}
	FScopeLock Lock(&GMutex);
	const FStreak* Streak = GStreaks.Find(Owner->Principal);
	if (!Streak || Streak->Fingerprint != Owner->Fingerprint || Streak->Count < LoopLimit ||
		FPlatformTime::Seconds() - Streak->At >= LoopWindowSeconds)
	{
		return FString();
	}
	return FString::Printf(TEXT("This exact call failed %d times in a row (%s) and was not run again: sent unchanged it fails the same way. ")
		TEXT("Change its params as that error said, run another call first (a read of what it named), or read the contract with nextCall."),
		Streak->Count, *Streak->Code);
}

void McpLoopGuardNote(const TSharedPtr<FMcpPageOwner>& Owner, bool bSuccess, const FString& ErrorCode)
{
	using namespace McpNativeLoopGuard;
	if (!Owner.IsValid())
	{
		return;
	}
	const FString Code = ErrorCode.IsEmpty() ? FString(TEXT("UNREAL_EXECUTION_ERROR")) : ErrorCode;
	FScopeLock Lock(&GMutex);
	if (bSuccess || IsMomentCode(Code))
	{
		GStreaks.Remove(Owner->Principal);
		return;
	}
	const double Now = FPlatformTime::Seconds();
	FStreak& Streak = GStreaks.FindOrAdd(Owner->Principal);
	const bool bRepeat = Streak.Count > 0 && Streak.Fingerprint == Owner->Fingerprint && Now - Streak.At < LoopWindowSeconds;
	Streak.Count = bRepeat ? Streak.Count + 1 : 1;
	Streak.Fingerprint = Owner->Fingerprint;
	Streak.Code = Code;
	Streak.At = Now;
}

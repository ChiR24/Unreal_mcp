#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

#include "Foundation/McpCapabilityPrincipal.h"

// Task 40 principal-wide quota ledger.
//
// Quota is keyed by the STABLE principal identity ("scoped:<profile>", "legacy",
// "loopback"), never by socket or session. That is the whole point: the existing
// per-socket rate limit in FMcpConnectionManager resets the moment a client
// reconnects, and a native client could fan out across sessions. Keying on the
// principal makes reconnect and session fan-out useless as a bypass, and keeps
// one principal's traffic from consuming another's budget.
//
// The ledger is process-wide and shared by BOTH transports, so a principal that
// spends its budget over the WebSocket bridge has already spent it for native
// /mcp. It is bounded: at most MaxTrackedPrincipals identities are tracked, with
// least-recently-seen eviction, so a hostile client cannot grow it without limit.
class FMcpPrincipalQuotaLedger
{
public:
	/** Process-wide ledger shared by the WebSocket bridge and native /mcp. */
	static FMcpPrincipalQuotaLedger& Get();

	/**
	 * Charge one request against the principal's budget.
	 * A limit of 0 means unlimited, which is what every unrestricted principal
	 * (loopback, legacy admin) carries by default, so behaviour is unchanged
	 * unless a scoped token configures a limit.
	 * Returns true when the charge fits; on false OutReason carries a
	 * caller-safe explanation that never contains a token.
	 */
	bool TryCharge(const FMcpCapabilityPrincipal& Principal, bool bIsToolCall, FString& OutReason);

	/** Drop all windows. Used by automation tests to isolate cases. */
	void Reset();

	/** Tracked identity count, for the bounded-growth automation test. */
	int32 GetTrackedPrincipalCount();

	static constexpr int32 MaxTrackedPrincipals = 256;
	static constexpr double WindowSeconds = 60.0;

private:
	struct FQuotaWindow
	{
		double WindowStartSeconds = 0.0;
		double LastSeenSeconds = 0.0;
		int32 Requests = 0;
		int32 ToolCalls = 0;
	};

	// Caller must hold Mutex. Evicts the least-recently-seen identity when the
	// ledger is at capacity and a new identity needs a slot.
	void EvictOldestIfFull();

	FCriticalSection Mutex;
	TMap<FString, FQuotaWindow> Windows;
};

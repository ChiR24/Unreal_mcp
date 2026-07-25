#include "Foundation/McpPrincipalQuota.h"

#include "HAL/PlatformTime.h"

FMcpPrincipalQuotaLedger& FMcpPrincipalQuotaLedger::Get()
{
	static FMcpPrincipalQuotaLedger Ledger;
	return Ledger;
}

void FMcpPrincipalQuotaLedger::Reset()
{
	FScopeLock Lock(&Mutex);
	Windows.Empty();
}

int32 FMcpPrincipalQuotaLedger::GetTrackedPrincipalCount()
{
	FScopeLock Lock(&Mutex);
	return Windows.Num();
}

void FMcpPrincipalQuotaLedger::EvictOldestIfFull()
{
	if (Windows.Num() < MaxTrackedPrincipals)
	{
		return;
	}
	const FString* OldestKey = nullptr;
	double OldestSeen = TNumericLimits<double>::Max();
	for (const TPair<FString, FQuotaWindow>& Pair : Windows)
	{
		if (Pair.Value.LastSeenSeconds < OldestSeen)
		{
			OldestSeen = Pair.Value.LastSeenSeconds;
			OldestKey = &Pair.Key;
		}
	}
	if (OldestKey)
	{
		Windows.Remove(*OldestKey);
	}
}

bool FMcpPrincipalQuotaLedger::TryCharge(
	const FMcpCapabilityPrincipal& Principal, bool bIsToolCall, FString& OutReason)
{
	const int32 RequestLimit = Principal.MaxRequestsPerMinute;
	const int32 ToolCallLimit = Principal.MaxToolCallsPerMinute;
	// An unconfigured limit means unlimited, so an unrestricted legacy/loopback
	// admin never touches the ledger and can never be evicted from it.
	if (RequestLimit <= 0 && ToolCallLimit <= 0)
	{
		return true;
	}

	const double Now = FPlatformTime::Seconds();

	FScopeLock Lock(&Mutex);
	FQuotaWindow* Window = Windows.Find(Principal.Identity);
	if (!Window)
	{
		EvictOldestIfFull();
		Window = &Windows.Add(Principal.Identity, FQuotaWindow{});
		Window->WindowStartSeconds = Now;
	}

	// Fixed 60-second window anchored to the principal's first charge. Once the
	// window elapses the counters restart; the window is NOT reset by a
	// reconnect, which is what closes the reconnect bypass.
	if (Now - Window->WindowStartSeconds >= WindowSeconds)
	{
		Window->WindowStartSeconds = Now;
		Window->Requests = 0;
		Window->ToolCalls = 0;
	}
	Window->LastSeenSeconds = Now;

	const int32 NextRequests = Window->Requests + 1;
	const int32 NextToolCalls = Window->ToolCalls + (bIsToolCall ? 1 : 0);

	if (RequestLimit > 0 && NextRequests > RequestLimit)
	{
		OutReason = FString::Printf(
			TEXT("Request quota of %d per minute exhausted for this principal."), RequestLimit);
		return false;
	}
	if (bIsToolCall && ToolCallLimit > 0 && NextToolCalls > ToolCallLimit)
	{
		OutReason = FString::Printf(
			TEXT("Tool-call quota of %d per minute exhausted for this principal."), ToolCallLimit);
		return false;
	}

	// Commit only on success, so a refused request does not consume budget.
	Window->Requests = NextRequests;
	Window->ToolCalls = NextToolCalls;
	return true;
}

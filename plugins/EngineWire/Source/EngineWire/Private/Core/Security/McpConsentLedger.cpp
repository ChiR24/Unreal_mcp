#include "Core/Security/McpConsentLedger.h"

#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"

namespace McpCapabilityAuthorization
{
namespace
{
// Bounded oldest-first, so a long-lived editor never grows either map without limit. A pruned issued nonce is
// refused on the native door (describe again); a pruned consumed one becomes presentable again only where issuance is
// not checked, and only for a call whose grant still names its capability.
void McpRememberNonce(TMap<FString, FString>& Map, TArray<FString>& Order, const FString& Nonce,
                      const FString& Capability, int32 MaxEntries)
{
	if (Order.Num() >= MaxEntries)
	{
		Map.Remove(Order[0]);
		Order.RemoveAt(0);
	}
	Order.Add(Nonce);
	Map.Add(Nonce, Capability);
}
} // namespace

FMcpConsentLedger& FMcpConsentLedger::Get()
{
	static FMcpConsentLedger Instance;
	return Instance;
}

FString FMcpConsentLedger::Mint(const FString& CapabilityId)
{
	const FString Nonce = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	FScopeLock Lock(&Mutex);
	McpRememberNonce(Issued, IssueOrder, Nonce, CapabilityId, MaxEntries);
	return Nonce;
}

EMcpConsentNonce FMcpConsentLedger::Consume(const FString& Nonce, const FString& CapabilityId, bool bRequireIssued)
{
	if (Nonce.IsEmpty())
	{
		return bRequireIssued ? EMcpConsentNonce::NotIssued : EMcpConsentNonce::Accepted;
	}
	FScopeLock Lock(&Mutex);
	if (Consumed.Contains(Nonce))
	{
		return EMcpConsentNonce::Reused;
	}
	const FString* IssuedFor = Issued.Find(Nonce);
	if (bRequireIssued && (!IssuedFor || !IssuedFor->Equals(CapabilityId, ESearchCase::CaseSensitive)))
	{
		return EMcpConsentNonce::NotIssued;
	}
	const FString Capability = IssuedFor ? *IssuedFor : FString();
	if (IssuedFor)
	{
		Issued.Remove(Nonce);
		IssueOrder.RemoveSingle(Nonce);
	}
	McpRememberNonce(Consumed, ConsumptionOrder, Nonce, Capability, MaxEntries);
	return EMcpConsentNonce::Accepted;
}

void FMcpConsentLedger::Refund(const FString& Nonce)
{
	if (Nonce.IsEmpty())
	{
		return;
	}
	FScopeLock Lock(&Mutex);
	FString Capability;
	if (Consumed.RemoveAndCopyValue(Nonce, Capability))
	{
		ConsumptionOrder.RemoveSingle(Nonce);
		if (!Capability.IsEmpty())
		{
			McpRememberNonce(Issued, IssueOrder, Nonce, Capability, MaxEntries);
		}
	}
}
} // namespace McpCapabilityAuthorization

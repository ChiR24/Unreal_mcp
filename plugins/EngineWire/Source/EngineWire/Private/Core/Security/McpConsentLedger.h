#pragma once

#include "CoreMinimal.h"

// The single-use ledger behind consent grants; both transports authorize through the pre-queue gate, which burns a
// grant's nonce on first acceptance, so a replayed grant is CONSENT_REUSED. describe and the 2026-07-28 consent ask
// mint every nonce here, bound to the capability it was minted for, and the native door accepts only such a nonce
// (CONSENT_NOT_ISSUED otherwise): a made-up nonce used to be taken as fresh and a missing one as legacy. The WebSocket
// door has no describe, so it keeps the older rule. Thread-safe.
namespace McpCapabilityAuthorization
{
enum class EMcpConsentNonce : uint8
{
	Accepted,
	Reused,
	NotIssued
};

class FMcpConsentLedger
{
public:
	static FMcpConsentLedger& Get();

	/** A fresh nonce for a grant of CapabilityId, recorded as issued. */
	FString Mint(const FString& CapabilityId);

	/**
	 * Burns Nonce on first presentation. bRequireIssued (the native door): it must have been minted here for
	 * CapabilityId, so an empty, made-up, too old or other capability's nonce is NotIssued. Without it an empty nonce
	 * passes and an unseen one is accepted.
	 */
	EMcpConsentNonce Consume(const FString& Nonce, const FString& CapabilityId, bool bRequireIssued);

	/** The rule without issuance: true when Nonce is empty or burns now. */
	bool TryConsume(const FString& Nonce) { return Consume(Nonce, FString(), false) == EMcpConsentNonce::Accepted; }

	// Hands a burned nonce back, issued again when it was minted here. The gate burns before the handler runs, so a
	// handler that refuses without changing anything (a typo'd name, a bad path) would otherwise cost the caller its
	// single-use grant and a describe round trip.
	void Refund(const FString& Nonce);

private:
	FMcpConsentLedger() = default;
	FCriticalSection Mutex;
	TMap<FString, FString> Issued; // nonce -> the capability it was minted for, until burned
	TArray<FString> IssueOrder;
	TMap<FString, FString> Consumed; // nonce -> that capability (empty when it was never minted here)
	TArray<FString> ConsumptionOrder;
	static constexpr int32 MaxEntries = 4096;
};
} // namespace McpCapabilityAuthorization

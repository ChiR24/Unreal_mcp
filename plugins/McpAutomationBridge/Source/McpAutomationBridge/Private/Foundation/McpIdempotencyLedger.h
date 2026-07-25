#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

// Task 41 principal-scoped idempotency ledger (native mirror of the TypeScript
// src/server/gateway/idempotency-ledger.ts, at 4096 entries rather than 1024).
//
// A slot is keyed by a SHA-256 digest over principal + capability + the client's
// idempotency key, so the raw key never enters the map and can never reach a log
// line, a receipt or an evidence file. Binding the principal into the digest is
// what stops one principal replaying, colliding with, or observing another's
// execution.
//
// Two properties are load-bearing and match the TypeScript side exactly:
//   * A failure is NEVER recorded. Abandon() drops the slot outright, so a
//     transient error stays retryable and an authorization/validation/consent
//     refusal (which is refused before it ever reaches Begin) can never be
//     replayed as a successful receipt.
//   * Eviction only ever removes a COMPLETED entry. Dropping an in-flight entry
//     would admit a concurrent duplicate as a second real dispatch, which is
//     precisely the mutation this ledger exists to prevent.
//
// Locking: every method takes the mutex for a short, editor-free critical
// section and releases it before returning. Callers therefore hold no lock
// across dispatch, which is what keeps game-thread editor work off this lock.
//
// State is process-local and deliberately never persisted: after a restart the
// editor may have changed underneath a recorded receipt, so forgetting is the
// only safe behaviour.
enum class EMcpIdempotencyOutcome : uint8
{
	/** Caller owns the slot and must dispatch, then Complete() or Abandon(). */
	First,
	/** An identical request is still executing; refuse the duplicate. */
	InFlight,
	/** A completed identical request exists; return OutReplayReceipt verbatim. */
	Replay,
	/** The key was reused with a different fingerprint; refuse without leaking. */
	Conflict,
	/** Dedup does not apply (no key, or the digest could not be computed). */
	Disabled
};

class FMcpIdempotencyLedger
{
public:
	/** Process-wide ledger shared by the WebSocket bridge and native /mcp. */
	static FMcpIdempotencyLedger& Get();

	/**
	 * Claim a slot for one execution. Fingerprint must be computed from the
	 * POST-normalization params so a key replayed with different effective
	 * arguments is a Conflict rather than a silent replay.
	 * On Replay, OutReplayReceipt carries the recorded receipt JSON.
	 * On First, OutSlot carries the handle for Complete()/Abandon().
	 */
	EMcpIdempotencyOutcome Begin(
		const FString& PrincipalIdentity,
		const FString& CapabilityId,
		const FString& IdempotencyKey,
		const FString& Fingerprint,
		FString& OutSlot,
		FString& OutReplayReceipt);

	/** Record the receipt for a finished execution and enforce the cap. */
	void Complete(const FString& Slot, const FString& ReceiptJson);

	/** Release a slot without recording anything, keeping the key retryable. */
	void Abandon(const FString& Slot);

	/** Drop every entry. Used by automation tests to isolate cases. */
	void Reset();

	/** An unset TFunction restores real time. */
	void SetClockForTests(TFunction<double()> InClock);

	/** Entry count, for the bounded-growth automation test. */
	int32 GetEntryCount();

	/** Digest of principal + capability + key; public so tests can pin it. */
	static bool ComputeSlot(
		const FString& PrincipalIdentity,
		const FString& CapabilityId,
		const FString& IdempotencyKey,
		FString& OutSlot);

	static constexpr int32 MaxEntries = 4096;
	static constexpr double TtlSeconds = 24.0 * 60.0 * 60.0;

private:
	struct FEntry
	{
		FString Fingerprint;
		FString ReceiptJson;
		double CompletedAtSeconds = 0.0;
		uint64 Sequence = 0;
		bool bCompleted = false;
	};

	// Caller must hold Mutex. TMap has no insertion order, so eviction picks the
	// lowest sequence among COMPLETED entries to stay deterministic.
	void EvictCompletedOverCap();

	double NowSeconds() const;

	FCriticalSection Mutex;
	TMap<FString, FEntry> Entries;
	TFunction<double()> ClockOverride;
	uint64 NextSequence = 1;
};

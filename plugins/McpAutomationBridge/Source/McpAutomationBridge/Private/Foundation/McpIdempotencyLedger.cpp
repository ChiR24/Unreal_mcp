#include "Foundation/McpIdempotencyLedger.h"

#include "Containers/StringConv.h"
#include "HAL/PlatformTime.h"
#include "openssl/sha.h"

namespace
{
	const TCHAR* const FieldSeparator = TEXT(" ");
}

FMcpIdempotencyLedger& FMcpIdempotencyLedger::Get()
{
	static FMcpIdempotencyLedger Ledger;
	return Ledger;
}

bool FMcpIdempotencyLedger::ComputeSlot(
	const FString& PrincipalIdentity,
	const FString& CapabilityId,
	const FString& IdempotencyKey,
	FString& OutSlot)
{
	const FString Combined = PrincipalIdentity + FieldSeparator + CapabilityId + FieldSeparator + IdempotencyKey;
	const FTCHARToUTF8 Utf8(*Combined);
	// OpenSSL, not FPlatformMisc::GetSHA256Signature: the engine's is checkf(false)
	// on this platform and aborts. This mirrors the plugin's Python handler, which
	// already links and uses OpenSSL SHA256 for the same digest need.
	unsigned char Hash[SHA256_DIGEST_LENGTH];
	SHA256(reinterpret_cast<const unsigned char*>(Utf8.Get()), static_cast<size_t>(Utf8.Length()), Hash);
	FString Digest;
	Digest.Reserve(SHA256_DIGEST_LENGTH * 2);
	for (int32 Index = 0; Index < SHA256_DIGEST_LENGTH; ++Index)
	{
		Digest += FString::Printf(TEXT("%02x"), Hash[Index]);
	}
	OutSlot = MoveTemp(Digest);
	return true;
}

void FMcpIdempotencyLedger::Reset()
{
	FScopeLock Lock(&Mutex);
	Entries.Empty();
	ClockOverride.Reset();
	NextSequence = 1;
}

void FMcpIdempotencyLedger::SetClockForTests(TFunction<double()> InClock)
{
	FScopeLock Lock(&Mutex);
	ClockOverride = MoveTemp(InClock);
}

double FMcpIdempotencyLedger::NowSeconds() const
{
	return ClockOverride ? ClockOverride() : FPlatformTime::Seconds();
}

int32 FMcpIdempotencyLedger::GetEntryCount()
{
	FScopeLock Lock(&Mutex);
	return Entries.Num();
}

EMcpIdempotencyOutcome FMcpIdempotencyLedger::Begin(
	const FString& PrincipalIdentity,
	const FString& CapabilityId,
	const FString& IdempotencyKey,
	const FString& Fingerprint,
	FString& OutSlot,
	FString& OutReplayReceipt)
{
	OutReplayReceipt.Reset();
	if (IdempotencyKey.IsEmpty())
	{
		return EMcpIdempotencyOutcome::Disabled;
	}
	FString Slot;
	if (!ComputeSlot(PrincipalIdentity, CapabilityId, IdempotencyKey, Slot))
	{
		return EMcpIdempotencyOutcome::Disabled;
	}

	FScopeLock Lock(&Mutex);
	const double Now = NowSeconds();
	FEntry* Existing = Entries.Find(Slot);
	if (Existing && Existing->bCompleted && Now - Existing->CompletedAtSeconds > TtlSeconds)
	{
		Entries.Remove(Slot);
		Existing = nullptr;
	}

	if (!Existing)
	{
		FEntry Fresh;
		Fresh.Fingerprint = Fingerprint;
		Fresh.Sequence = NextSequence++;
		Entries.Add(Slot, MoveTemp(Fresh));
		OutSlot = Slot;
		return EMcpIdempotencyOutcome::First;
	}

	if (Existing->Fingerprint != Fingerprint)
	{
		// No recorded receipt is returned: a caller that guessed another's key
		// must not learn what that call produced.
		return EMcpIdempotencyOutcome::Conflict;
	}
	if (!Existing->bCompleted)
	{
		return EMcpIdempotencyOutcome::InFlight;
	}
	OutReplayReceipt = Existing->ReceiptJson;
	return EMcpIdempotencyOutcome::Replay;
}

void FMcpIdempotencyLedger::Complete(const FString& Slot, const FString& ReceiptJson)
{
	if (Slot.IsEmpty())
	{
		return;
	}
	FScopeLock Lock(&Mutex);
	FEntry* Entry = Entries.Find(Slot);
	if (!Entry || Entry->bCompleted)
	{
		return;
	}
	Entry->bCompleted = true;
	Entry->ReceiptJson = ReceiptJson;
	Entry->CompletedAtSeconds = NowSeconds();
	EvictCompletedOverCap();
}

void FMcpIdempotencyLedger::Abandon(const FString& Slot)
{
	if (Slot.IsEmpty())
	{
		return;
	}
	FScopeLock Lock(&Mutex);
	Entries.Remove(Slot);
}

void FMcpIdempotencyLedger::EvictCompletedOverCap()
{
	int32 CompletedCount = 0;
	for (const TPair<FString, FEntry>& Pair : Entries)
	{
		if (Pair.Value.bCompleted)
		{
			++CompletedCount;
		}
	}

	while (CompletedCount > MaxEntries)
	{
		const FString* OldestKey = nullptr;
		uint64 OldestSequence = TNumericLimits<uint64>::Max();
		for (const TPair<FString, FEntry>& Pair : Entries)
		{
			if (Pair.Value.bCompleted && Pair.Value.Sequence < OldestSequence)
			{
				OldestSequence = Pair.Value.Sequence;
				OldestKey = &Pair.Key;
			}
		}
		if (!OldestKey)
		{
			return;
		}
		Entries.Remove(*OldestKey);
		--CompletedCount;
	}
}

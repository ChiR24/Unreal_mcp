#include "Foundation/McpIdempotencyLedger.h"

#include "Containers/StringConv.h"
#include "HAL/PlatformTime.h"
#include "openssl/sha.h"

namespace
{
	// Netstring-style field encoding: <utf8ByteLength> ':' <utf8Bytes>. Gluing the
	// fields with a single delimiter is NOT injective - a delimiter occurring
	// inside a field is indistinguishable from a field boundary, and a space is a
	// legal character in an operator-configured scoped-token profile, so a crafted
	// principal could shift bytes across a boundary and land two distinct scopes on
	// one slot. Prefixing each field with its own byte length removes the ambiguity.
	//
	// This is byte-for-byte the encoding the TypeScript mirror builds in
	// src/server/gateway/idempotency-ledger.ts, so both surfaces digest the same
	// preimage for the same inputs; the vectors below are pinned on both sides.
	// FString is NUL-terminated, so a field carrying an embedded NUL is truncated
	// here before encoding - the encoding stays injective over what is encoded.
	void AppendLengthPrefixedField(TArray<uint8>& Out, const FString& Field)
	{
		const FTCHARToUTF8 Utf8(*Field);
		const int32 ByteLength = Utf8.Length();
		const FString Prefix = FString::Printf(TEXT("%d:"), ByteLength);
		const FTCHARToUTF8 PrefixUtf8(*Prefix);
		Out.Append(reinterpret_cast<const uint8*>(PrefixUtf8.Get()), PrefixUtf8.Length());
		Out.Append(reinterpret_cast<const uint8*>(Utf8.Get()), ByteLength);
	}
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
	TArray<uint8> Preimage;
	AppendLengthPrefixedField(Preimage, PrincipalIdentity);
	AppendLengthPrefixedField(Preimage, CapabilityId);
	AppendLengthPrefixedField(Preimage, IdempotencyKey);
	// OpenSSL, not FPlatformMisc::GetSHA256Signature: the engine's is checkf(false)
	// on this platform and aborts. This mirrors the plugin's Python handler, which
	// already links and uses OpenSSL SHA256 for the same digest need.
	unsigned char Hash[SHA256_DIGEST_LENGTH];
	SHA256(reinterpret_cast<const unsigned char*>(Preimage.GetData()), static_cast<size_t>(Preimage.Num()), Hash);
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
		// Copy before removing: OldestKey aliases the FString owned BY the map
		// element, and TMap::Remove destroys that element (freeing the string's
		// buffer) while still holding the reference it was handed.
		const FString EvictKey = *OldestKey;
		Entries.Remove(EvictKey);
		--CompletedCount;
	}
}

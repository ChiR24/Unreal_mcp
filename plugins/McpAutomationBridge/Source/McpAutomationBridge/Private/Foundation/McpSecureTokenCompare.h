#pragma once

#include "CoreMinimal.h"

/**
 * Constant-time, length-safe capability-token comparison for the native MCP
 * HTTP transport and the WebSocket bridge_hello handshake.
 *
 * Both transports gate access on a shared secret (X-MCP-Capability-Token /
 * bridge_hello.capabilityToken). A naive `A != B` short-circuits on the first
 * differing byte, so an attacker probing token values learns (via timing) how
 * many leading bytes matched — a classic timing oracle. This helper instead
 * walks the full UTF-8 byte span of both operands and XOR-accumulates the
 * differences, returning true only when every compared byte (and the lengths)
 * agree. There is no data-dependent early exit on the byte comparison, so the
 * comparison time does not reveal how much of the token matched.
 *
 * Lengths are folded into the accumulator (LenA ^ LenB) rather than used to
 * short-circuit, so an empty or shorter candidate still compares unequal
 * without branching on the length difference. The accumulator is uint32-wide
 * because the length XOR does not fit in a byte: narrowing it truncates, and
 * any length pair whose XOR is a nonzero multiple of 256 (1 vs 257, 16 vs 272)
 * would fold to zero and lose the mismatch entirely.
 */
inline bool McpConstantTimeTokenEquals(const FString& A, const FString& B)
{
	FTCHARToUTF8 Utf8A(*A);
	FTCHARToUTF8 Utf8B(*B);
	const int32 LenA = Utf8A.Length();
	const int32 LenB = Utf8B.Length();
	const int32 MaxLen = FMath::Max(LenA, LenB);

	// Start the accumulator from the length difference so length mismatches
	// are captured without an early-out branch. Full width, not uint8: the XOR
	// of two lengths can carry its only set bits above bit 7.
	uint32 Diff = static_cast<uint32>(LenA ^ LenB);

	const uint8* Pa = reinterpret_cast<const uint8*>(Utf8A.Get());
	const uint8* Pb = reinterpret_cast<const uint8*>(Utf8B.Get());
	for (int32 i = 0; i < MaxLen; ++i)
	{
		const uint8 Ba = (i < LenA) ? Pa[i] : static_cast<uint8>(0);
		const uint8 Bb = (i < LenB) ? Pb[i] : static_cast<uint8>(0);
		// OR (not XOR-assign) so a zero byte in one operand cannot mask a
		// difference: Diff stays nonzero once any byte ever differed.
		Diff |= static_cast<uint8>(Ba ^ Bb);
	}

	return Diff == 0;
}

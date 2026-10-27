// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

// Programme loudness as EBU R128 and ITU-R BS.1770-4 define it, written from libebur128's design (MIT): each channel
// K-weighted (a high shelf and a high pass, here one fourth-order filter), its mean square taken over 400 ms blocks every
// 100 ms, the blocks gated at -70 LUFS and then at 10 LU under the mean of those left.
namespace McpLoudness
{
/** Integrated loudness in LUFS of interleaved samples (-1 to 1); unset when no block clears the -70 LUFS gate (silence,
 *  or under 400 ms of sound). Channels in the engine's order: from six channels up the fourth (low frequency) is left
 *  out and the surrounds weigh 1.41; mono is one channel, so a mono sine reads 3 LU under the same sine in stereo. */
TOptional<double> Integrated(const TArray<float>& Interleaved, int32 Channels, int32 SampleRate);
} // namespace McpLoudness

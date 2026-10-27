// Copyright (c) 2024 EngineWire Contributors

#include "Domains/AudioAuthoring/McpLoudness.h"

namespace McpLoudness
{
namespace
{
constexpr double LoudnessPi = 3.14159265358979323846;
constexpr double LoudnessAbsoluteGate = -70.0;
constexpr double LoudnessRelativeGate = -10.0;

struct FLoudnessFilter
{
    double B[5] = {};
    double A[5] = {};
};

// The K-weighting filter for a sample rate: libebur128's analogue prototypes (a +4 dB shelf above 1.68 kHz, a high pass
// at 38 Hz) through the bilinear transform, the two sections multiplied into one.
FLoudnessFilter LoudnessFilterFor(double Rate)
{
    double F0 = 1681.974450955533, Q = 0.7071752369554196;
    double K = FMath::Tan(LoudnessPi * F0 / Rate);
    const double Vh = FMath::Pow(10.0, 3.999843853973347 / 20.0), Vb = FMath::Pow(Vh, 0.4996667741545416);
    const double A0 = 1.0 + K / Q + K * K;
    const double Pb[3] = {(Vh + Vb * K / Q + K * K) / A0, 2.0 * (K * K - Vh) / A0, (Vh - Vb * K / Q + K * K) / A0};
    const double Pa[3] = {1.0, 2.0 * (K * K - 1.0) / A0, (1.0 - K / Q + K * K) / A0};
    F0 = 38.13547087602444;
    Q = 0.5003270373238773;
    K = FMath::Tan(LoudnessPi * F0 / Rate);
    const double Rb[3] = {1.0, -2.0, 1.0};
    const double Ra[3] = {1.0, 2.0 * (K * K - 1.0) / (1.0 + K / Q + K * K), (1.0 - K / Q + K * K) / (1.0 + K / Q + K * K)};
    FLoudnessFilter Out;
    for (int32 I = 0; I < 3; ++I)
    {
        for (int32 J = 0; J < 3; ++J)
        {
            Out.B[I + J] += Pb[I] * Rb[J];
            Out.A[I + J] += Pa[I] * Ra[J];
        }
    }
    return Out;
}

double LoudnessWeight(int32 Channel, int32 Channels)
{
    return Channels < 6 ? 1.0 : Channel == 3 ? 0.0 : Channel >= 4 ? 1.41 : 1.0;
}

double LoudnessOf(double MeanSquare)
{
    return -0.691 + 10.0 * FMath::LogX(10.0, MeanSquare);
}

// The loudness of the mean of the blocks whose own loudness is above Gate; unset when none is.
TOptional<double> LoudnessAbove(const TArray<double>& Blocks, double Gate)
{
    double Sum = 0.0;
    int32 Count = 0;
    for (const double Block : Blocks)
    {
        if (Block > 0.0 && LoudnessOf(Block) > Gate)
        {
            Sum += Block;
            ++Count;
        }
    }
    return Count > 0 ? TOptional<double>(LoudnessOf(Sum / Count)) : TOptional<double>();
}
} // namespace

TOptional<double> Integrated(const TArray<float>& Interleaved, int32 Channels, int32 SampleRate)
{
    const int32 Hop = FMath::RoundToInt(SampleRate * 0.1); // blocks of four hops, one hop apart: 400 ms, 75% overlap
    const int32 Hops = Channels > 0 && Hop > 0 ? Interleaved.Num() / Channels / Hop : 0;
    if (Hops < 4)
    {
        return TOptional<double>();
    }
    const FLoudnessFilter Filter = LoudnessFilterFor(SampleRate);
    TArray<double> HopEnergy;
    HopEnergy.Init(0.0, Hops);
    for (int32 Channel = 0; Channel < Channels; ++Channel)
    {
        const double Weight = LoudnessWeight(Channel, Channels);
        double V[5] = {};
        for (int32 Frame = 0; Weight > 0.0 && Frame < Hops * Hop; ++Frame)
        {
            const double V0 = Interleaved[Frame * Channels + Channel] - Filter.A[1] * V[1] - Filter.A[2] * V[2] - Filter.A[3] * V[3] - Filter.A[4] * V[4];
            const double Y = Filter.B[0] * V0 + Filter.B[1] * V[1] + Filter.B[2] * V[2] + Filter.B[3] * V[3] + Filter.B[4] * V[4];
            V[4] = V[3];
            V[3] = V[2];
            V[2] = V[1];
            V[1] = V0;
            HopEnergy[Frame / Hop] += Weight * Y * Y;
        }
    }
    TArray<double> Blocks;
    for (int32 At = 3; At < Hops; ++At)
    {
        Blocks.Add((HopEnergy[At - 3] + HopEnergy[At - 2] + HopEnergy[At - 1] + HopEnergy[At]) / (4.0 * Hop));
    }
    const TOptional<double> Ungated = LoudnessAbove(Blocks, LoudnessAbsoluteGate);
    return Ungated.IsSet() ? LoudnessAbove(Blocks, FMath::Max(LoudnessAbsoluteGate, Ungated.GetValue() + LoudnessRelativeGate)) : Ungated;
}
} // namespace McpLoudness

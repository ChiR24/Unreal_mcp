// Copyright (c) 2024 EngineWire Contributors

#include "Domains/AudioAuthoring/McpLoudness.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpLoudnessTest, "EngineWire.Audio.Loudness",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
struct FLoudnessPart
{
    double Db;      // the sine's peak level in dBFS
    double Seconds;
};

// A 1 kHz sine in parts, the same on every channel.
TArray<float> LoudnessSine(int32 Channels, int32 Rate, const TArray<FLoudnessPart>& Parts)
{
    TArray<float> Out;
    double Seconds = 0.0;
    for (const FLoudnessPart& Part : Parts)
    {
        Seconds += Part.Seconds;
    }
    Out.Reserve(int32(Seconds * Rate) * Channels);
    int64 Sample = 0;
    for (const FLoudnessPart& Part : Parts)
    {
        const double Amplitude = FMath::Pow(10.0, Part.Db / 20.0);
        for (int64 Frame = 0; Frame < int64(Part.Seconds * Rate); ++Frame, ++Sample)
        {
            const float Value = float(Amplitude * FMath::Sin(2.0 * 3.14159265358979323846 * 1000.0 * Sample / Rate));
            for (int32 Channel = 0; Channel < Channels; ++Channel)
            {
                Out.Add(Value);
            }
        }
    }
    return Out;
}

double LoudnessOrSilence(const TOptional<double>& Value)
{
    return Value.IsSet() ? Value.GetValue() : -1000.0;
}
} // namespace

// Known answers of EBU Tech 3341: a stereo 1 kHz sine at -23 dBFS reads -23 LUFS at 48 and 44.1 kHz, -33 dBFS reads -33
// (steady sines shortened to 5 s, which read the same as 20); the -36 dBFS parts around a -23 dBFS one fall under the
// relative gate (at the test's own 10, 60 and 10 s: the blocks straddling each step stay above the gate, so 2, 10 and 2 s
// read -23.12); a mono sine counts one channel (3 LU under stereo); silence and 300 ms have no loudness.
bool FMcpLoudnessTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using McpLoudness::Integrated;
    TestEqual(TEXT("stereo -23 dBFS at 48 kHz"), LoudnessOrSilence(Integrated(LoudnessSine(2, 48000, {{-23.0, 5.0}}), 2, 48000)), -23.0, 0.1);
    TestEqual(TEXT("stereo -33 dBFS"), LoudnessOrSilence(Integrated(LoudnessSine(2, 48000, {{-33.0, 5.0}}), 2, 48000)), -33.0, 0.1);
    TestEqual(TEXT("stereo -23 dBFS at 44.1 kHz"), LoudnessOrSilence(Integrated(LoudnessSine(2, 44100, {{-23.0, 5.0}}), 2, 44100)), -23.0, 0.1);
    TestEqual(TEXT("quiet parts fall under the relative gate"),
              LoudnessOrSilence(Integrated(LoudnessSine(2, 48000, {{-36.0, 10.0}, {-23.0, 60.0}, {-36.0, 10.0}}), 2, 48000)), -23.0, 0.1);
    TestEqual(TEXT("mono counts one channel"), LoudnessOrSilence(Integrated(LoudnessSine(1, 48000, {{-23.0, 5.0}}), 1, 48000)), -26.0, 0.1);
    TArray<float> Silence;
    Silence.Init(0.f, 2 * 48000 * 2);
    TestFalse(TEXT("silence has no loudness"), Integrated(Silence, 2, 48000).IsSet());
    TestFalse(TEXT("300 ms is under one block"), Integrated(LoudnessSine(2, 48000, {{-23.0, 0.3}}), 2, 48000).IsSet());
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

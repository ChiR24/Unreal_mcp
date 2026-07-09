#include "Domains/Sequence/RecordReplay/McpAutomationBridge_SequenceReplayInternal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

namespace McpSequenceRecordReplay
{
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpReplayNameValidationTest,
    "McpAutomationBridge.Sequence.RecordReplay.ValidatesReplayNames",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpReplayNameValidationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TestTrue(TEXT("simple name"), IsSafeReplayName(TEXT("CinematicsReplay_123")));
    TestTrue(TEXT("hyphenated name"), IsSafeReplayName(TEXT("Replay-1")));

    const TArray<FString> InvalidNames = {
        TEXT(""), TEXT("-Replay"), TEXT("../Replay"), TEXT("/tmp/Replay"),
        TEXT("Folder\\Replay"), TEXT("Replay.demo"), TEXT("Replay?flush"),
        TEXT("Replay;quit"), TEXT("Replay\nName"), TEXT("Replay Name"),
        TEXT("\u00c9Replay")
    };
    for (const FString& Name : InvalidNames)
    {
        TestFalse(FString::Printf(TEXT("reject '%s'"), *Name), IsSafeReplayName(Name));
    }
    TestFalse(TEXT("overlong name"), IsSafeReplayName(
        FString::ChrN(129, TEXT('A'))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpReplayOptionValidationTest,
    "McpAutomationBridge.Sequence.RecordReplay.ValidatesReplayOptions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpReplayOptionValidationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FString Error;
    TestTrue(TEXT("empty options"), ValidateReplayOptions({}, Error));
    TestTrue(TEXT("null streamer"), ValidateReplayOptions(
        {TEXT("ReplayStreamerOverride=Null")}, Error));

    const TArray<FString> InvalidOptions = {
        TEXT("ReplayStreamerDemoPath=/tmp/owned"),
        TEXT("DemoPath=/tmp/owned"),
        TEXT("ReplayStreamerOverride=LocalFile"),
        TEXT("ReplayStreamerOverride=Null?ReplayStreamerDemoPath=/tmp/owned"),
        TEXT("flush"),
        TEXT("RecordMapChanges"),
        TEXT(";quit")
    };
    for (const FString& Option : InvalidOptions)
    {
        Error.Reset();
        TestFalse(
            FString::Printf(TEXT("reject '%s'"), *Option),
            ValidateReplayOptions({Option}, Error));
        TestFalse(TEXT("validation error is populated"), Error.IsEmpty());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpReplayRequestValidationTest,
    "McpAutomationBridge.Sequence.RecordReplay.ValidatesReplayRequests",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpReplayRequestValidationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FString Error;
    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("replayName"), TEXT("SafeReplay"));
    Payload->SetStringField(TEXT("demoName"), TEXT("../UnsafeAlias"));
    TestFalse(TEXT("all supplied name aliases are validated"),
        ValidateReplayRequest(Payload, Error));

    Payload = MakeShared<FJsonObject>();
    Payload->SetStringField(TEXT("replayName"), TEXT("SafeReplay"));
    TArray<TSharedPtr<FJsonValue>> Options;
    Options.Add(MakeShared<FJsonValueString>(
        TEXT("ReplayStreamerDemoPath=/tmp/owned")));
    Payload->SetArrayField(TEXT("additionalOptions"), Options);
    Error.Reset();
    TestFalse(TEXT("path-bearing option is rejected"),
        ValidateReplayRequest(Payload, Error));

    const TArray<TPair<FString, double>> InvalidNumbers = {
        {TEXT("checkpointSaveMaxMSPerFrame"), -1.0},
        {TEXT("checkpointSaveMaxMSPerFrame"), 0.0},
        {TEXT("maxRecordTimeSeconds"), -1.0},
        {TEXT("maxRecordTimeSeconds"), 0.0},
        {TEXT("playbackSpeed"), 0.0},
        {TEXT("durationSeconds"), 0.0},
        {TEXT("speed"), DBL_MAX},
        {TEXT("timeSeconds"), DBL_MAX},
        {TEXT("seconds"), DBL_MAX},
        {TEXT("checkpointSaveMaxMSPerFrame"), DBL_MAX},
        {TEXT("maxRecordTimeSeconds"), DBL_MAX},
        {TEXT("playbackSpeed"), DBL_MAX},
        {TEXT("durationSeconds"), DBL_MAX}
    };
    for (const TPair<FString, double>& Invalid : InvalidNumbers)
    {
        Payload = MakeShared<FJsonObject>();
        Payload->SetNumberField(Invalid.Key, Invalid.Value);
        Error.Reset();
        TestFalse(
            FString::Printf(TEXT("reject invalid %s"), *Invalid.Key),
            ValidateReplayRequest(Payload, Error));
        TestFalse(TEXT("numeric validation error is populated"), Error.IsEmpty());
    }

    Payload = MakeShared<FJsonObject>();
    Payload->SetNumberField(TEXT("checkpointSaveMaxMSPerFrame"), 5.0);
    Payload->SetNumberField(TEXT("maxRecordTimeSeconds"), 0.005);
    Payload->SetNumberField(TEXT("playbackSpeed"), 1.0);
    Payload->SetNumberField(TEXT("durationSeconds"), 1.0);
    Error.Reset();
    TestTrue(TEXT("valid numeric settings are accepted"),
        ValidateReplayRequest(Payload, Error));
    return true;
}
}

#endif

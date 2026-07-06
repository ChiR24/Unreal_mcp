#include "Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematics.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneFadeTrack.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpSequenceCinematicsTrackRollbackTest,
    "McpAutomationBridge.Sequence.Cinematics.TrackRollback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpSequenceCinematicsTrackRollbackTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    UMovieScene* MovieScene =
        NewObject<UMovieScene>(GetTransientPackage());
    if (!TestNotNull(TEXT("MovieScene exists"), MovieScene))
    {
        return false;
    }
    UMovieSceneFadeTrack* Track =
        MovieScene->AddTrack<UMovieSceneFadeTrack>();
    if (!TestNotNull(TEXT("Track exists"), Track))
    {
        return false;
    }
    TestTrue(TEXT("Track belongs to MovieScene"),
             MovieScene->ContainsTrack(*Track));
    McpSequenceCinematics::RemoveTrackAfterSectionFailure(
        MovieScene, Track, true);
    TestFalse(TEXT("Created track is removed after section failure"),
              MovieScene->ContainsTrack(*Track));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpSequenceCinematicsDisplayFrameConversionTest,
    "McpAutomationBridge.Sequence.Cinematics.DisplayFramesConvertToTicks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpSequenceCinematicsDisplayFrameConversionTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    UMovieScene* MovieScene =
        NewObject<UMovieScene>(GetTransientPackage());
    if (!TestNotNull(TEXT("MovieScene exists"), MovieScene))
    {
        return false;
    }
    MovieScene->SetDisplayRate(FFrameRate(24, 1));
    MovieScene->SetTickResolutionDirectly(FFrameRate(24000, 1));

    TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetNumberField(TEXT("startFrame"), 24);
    Payload->SetNumberField(TEXT("durationFrames"), 12);

    TestEqual(
        TEXT("One display-rate second converts to one tick-rate second"),
        McpSequenceCinematics::GetFrame(
            Payload, MovieScene, TEXT("startFrame")).Value,
        24000);
    TestEqual(
        TEXT("Display-frame duration converts to tick duration"),
        McpSequenceCinematics::GetDuration(Payload, MovieScene, 100),
        12000);

    MovieScene->SetDisplayRate(FFrameRate(24000, 1001));
    TestEqual(
        TEXT("Fractional display rates preserve exact frame-rate conversion"),
        McpSequenceCinematics::GetFrame(
            Payload, MovieScene, TEXT("startFrame")).Value,
        24024);
    return true;
}
#endif

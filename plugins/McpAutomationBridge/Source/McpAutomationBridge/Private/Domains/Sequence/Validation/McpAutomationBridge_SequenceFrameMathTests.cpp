#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceFrameMath.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "MovieScene.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpSequenceFrameMathTest,
    "McpAutomationBridge.Sequence.FrameMath.Boundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpSequenceFrameMathTest::RunTest(const FString &Parameters) {
  (void)Parameters;
  FString Error;
  FFrameNumber Frame;
  TestTrue(
      TEXT("safe transformed frame is accepted"),
      McpSequenceFrameMath::TryTransformFrame(
          24, FFrameRate(24, 1), FFrameRate(24000, 1), Frame, Error));
  TestEqual(TEXT("safe transformed frame value"), Frame.Value, 24000);

  Error.Reset();
  TestTrue(
      TEXT("fractional keyframe conversion is accepted"),
      McpSequenceFrameMath::TryTransformFrameFloor(
          1, FFrameRate(24, 1), FFrameRate(1000, 1), Frame, Error));
  TestEqual(TEXT("fractional keyframe conversion floors"), Frame.Value, 41);

  Error.Reset();
  TestTrue(
      TEXT("negative fractional keyframe conversion is accepted"),
      McpSequenceFrameMath::TryTransformFrameFloor(
          -1, FFrameRate(24, 1), FFrameRate(1000, 1), Frame, Error));
  TestEqual(
      TEXT("negative fractional keyframe conversion floors"),
      Frame.Value, -42);

  Error.Reset();
  TestTrue(TEXT("minimum frame endpoint is accepted"),
           McpSequenceFrameMath::TryFrameNumber(
               MIN_int32, Frame, Error));
  Error.Reset();
  TestTrue(TEXT("maximum frame endpoint is accepted"),
           McpSequenceFrameMath::TryFrameNumber(
               MAX_int32, Frame, Error));

  Error.Reset();
  TestFalse(
      TEXT("transformed maximum is rejected"),
      McpSequenceFrameMath::TryTransformFrame(
          MAX_int32, FFrameRate(24, 1), FFrameRate(24000, 1), Frame, Error));

  int32 Difference = 0;
  Error.Reset();
  TestFalse(
      TEXT("duration subtraction overflow is rejected"),
      McpSequenceFrameMath::TrySubtractFrames(
          MAX_int32, MIN_int32, Difference, Error));

  UMovieScene *MovieScene =
      NewObject<UMovieScene>(GetTransientPackage());
  if (!TestNotNull(TEXT("MovieScene exists"), MovieScene)) {
    return false;
  }
  MovieScene->SetDisplayRate(FFrameRate(1, 1));
  MovieScene->SetTickResolutionDirectly(FFrameRate(1, 1));
  TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
  Payload->SetNumberField(TEXT("startFrame"), MAX_int32);
  Payload->SetNumberField(TEXT("durationFrames"), 1);
  Error.Reset();
  TestFalse(
      TEXT("range endpoint overflow is rejected"),
      McpSequenceFrameMath::ValidateCinematicFrameRequest(
          Payload, MovieScene, Error));
  return true;
}
#endif

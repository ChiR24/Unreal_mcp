#include "Core/Compatibility/McpVersionCompatibility.h"

#if MCP_HAS_MOVIE_RENDER_PIPELINE && WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderInternal.h"
#include "Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderResourceLimits.h"
#include "MCP/Transport/McpNativeTransportTimeoutPolicy.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FrameRate.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpMovieRenderNativeTimeoutPolicyTest,
    "McpAutomationBridge.Sequence.MovieRender.NativeTimeoutPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpMovieRenderNativeTimeoutPolicyTest::RunTest(
    const FString &Parameters) {
  (void)Parameters;
  TSharedPtr<FJsonObject> RenderArguments = MakeShared<FJsonObject>();
  RenderArguments->SetStringField(TEXT("action"), TEXT("start_render"));
  TestEqual(
      TEXT("Default render timeout includes cancellation and response grace"),
      McpNativeTransportTimeoutPolicy::ResolveToolCallTimeoutSeconds(
          TEXT("manage_sequence"), RenderArguments, 3600000, 30000),
      335.0);
  RenderArguments->SetNumberField(TEXT("timeoutMs"), 3600000);
  TestEqual(
      TEXT("Maximum render timeout remains alive through cancellation"),
      McpNativeTransportTimeoutPolicy::ResolveToolCallTimeoutSeconds(
          TEXT("manage_sequence"), RenderArguments, 3600000, 30000),
      3635.0);
  TestEqual(
      TEXT("Other tool calls retain the standard timeout"),
      McpNativeTransportTimeoutPolicy::ResolveToolCallTimeoutSeconds(
          TEXT("manage_asset"), RenderArguments, 3600000, 30000),
      300.0);
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpMovieRenderFileNameFormatTest,
    "McpAutomationBridge.Sequence.MovieRender.FileNameFormatSecurity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpMovieRenderFileNameFormatTest::RunTest(const FString &Parameters) {
  FString Error;
  TestTrue(TEXT("MRQ tokens remain valid"),
           McpSequenceMovieRender::ValidateFileNameFormat(
               TEXT("{sequence_name}_{frame_number}"), Error));
  TestTrue(TEXT("Normal filename characters remain valid"),
           McpSequenceMovieRender::ValidateFileNameFormat(
               TEXT("Shot 01-final.v2"), Error));
  TestFalse(TEXT("Traversal is rejected"),
            McpSequenceMovieRender::ValidateFileNameFormat(
                TEXT("../escaped"), Error));
  TestFalse(TEXT("Forward-slash paths are rejected"),
            McpSequenceMovieRender::ValidateFileNameFormat(
                TEXT("nested/output"), Error));
  TestFalse(TEXT("Backslash paths are rejected"),
            McpSequenceMovieRender::ValidateFileNameFormat(
                TEXT("nested\\output"), Error));
  TestFalse(TEXT("Drive-qualified paths are rejected"),
            McpSequenceMovieRender::ValidateFileNameFormat(
                TEXT("C:output"), Error));
  TestFalse(TEXT("Directory-expanding tokens are rejected"),
            McpSequenceMovieRender::ValidateFileNameFormat(
                TEXT("{project_dir}"), Error));
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpMovieRenderValidationTest,
    "McpAutomationBridge.Sequence.MovieRender.RequestValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpMovieRenderValidationTest::RunTest(const FString &Parameters) {
  FString Error;
  TestTrue(TEXT("Simple render job names remain valid"),
           McpSequenceMovieRender::ValidateRenderJobName(
               TEXT("sequence automation Final.v2"), Error));
  TestFalse(TEXT("Job-name traversal is rejected"),
            McpSequenceMovieRender::ValidateRenderJobName(
                TEXT("../escaped"), Error));
  TestFalse(TEXT("Job-name format tokens are rejected"),
            McpSequenceMovieRender::ValidateRenderJobName(
                TEXT("{project_dir}"), Error));

  TSharedPtr<FJsonObject> NumericRate = MakeShared<FJsonObject>();
  NumericRate->SetNumberField(TEXT("frameRate"), 23.976);
  FString Code;
  TestTrue(TEXT("Numeric frame rates match the public schema"),
           McpSequenceMovieRender::ValidateOutputSettingsPayload(
               NumericRate, Error, Code));

  TSharedPtr<FJsonObject> InvalidRate = MakeShared<FJsonObject>();
  InvalidRate->SetNumberField(TEXT("frameRate"), 0.0);
  TestFalse(TEXT("Non-positive numeric frame rates are rejected"),
            McpSequenceMovieRender::ValidateOutputSettingsPayload(
                InvalidRate, Error, Code));
  TestEqual(TEXT("Invalid frame-rate error code"), Code,
            FString(TEXT("INVALID_FRAME_RATE")));
  return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpMovieRenderFrameAccountingTest,
    "McpAutomationBridge.Sequence.MovieRender.FrameAccounting",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpMovieRenderFrameAccountingTest::RunTest(
    const FString &Parameters) {
  const FFrameRate TickResolution(24000, 1);
  TestEqual(
      TEXT("Sequence tick ranges convert to effective output frames"),
      McpSequenceMovieRender::CalculateMovieRenderFrameCount(
          24000, TickResolution, TickResolution, FFrameRate(24, 1)),
      int64(24));
  TestEqual(
      TEXT("Custom display ranges pass through tick resolution"),
      McpSequenceMovieRender::CalculateMovieRenderFrameCount(
          24, FFrameRate(24, 1), TickResolution, FFrameRate(48, 1)),
      int64(48));
  TestEqual(
      TEXT("Unrepresentable intermediate tick spans are rejected"),
      McpSequenceMovieRender::CalculateMovieRenderFrameCount(
          10000, FFrameRate(1, 1), FFrameRate(1000000, 1),
          FFrameRate(1, 1)),
      MAX_int64);
  TestTrue(
      TEXT("Effective frame rate at the configured ceiling is accepted"),
      McpSequenceMovieRender::IsMovieRenderEffectiveFrameRateAllowed(
          FFrameRate(240, 1), 240));
  TestFalse(
      TEXT("Effective frame rate above the configured ceiling is rejected"),
      McpSequenceMovieRender::IsMovieRenderEffectiveFrameRateAllowed(
          FFrameRate(241, 1), 240));
  return true;
}

#endif

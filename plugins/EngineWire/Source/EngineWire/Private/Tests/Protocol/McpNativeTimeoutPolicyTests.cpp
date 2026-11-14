#include "MCP/Transport/McpNativeTransportTimeoutPolicy.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"

// The native transport's tool-call timeout: a render's own timeout plus the cancellation and response grace.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpMovieRenderNativeTimeoutPolicyTest,
    "EngineWire.Sequence.MovieRender.NativeTimeoutPolicy",
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
  RenderArguments->SetNumberField(TEXT("renderTimeoutMs"), 3600000);
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

#endif

#include "Domains/Sequence/Validation/McpAutomationBridge_SequenceIntegerValidation.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMcpSequenceIntegerValidationTest,
    "McpAutomationBridge.Sequence.IntegerBoundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpSequenceIntegerValidationTest::RunTest(
    const FString &Parameters) {
  (void)Parameters;
  FString Error;
  TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();

  Payload->SetNumberField(TEXT("frame"), MIN_int32);
  TestTrue(TEXT("MIN_int32 is accepted"),
           McpSequenceIntegerValidation::ValidateSequenceIntegerFields(
               Payload, Error));

  Payload->SetNumberField(TEXT("frame"), MAX_int32);
  Error.Reset();
  TestTrue(TEXT("MAX_int32 is accepted"),
           McpSequenceIntegerValidation::ValidateSequenceIntegerFields(
               Payload, Error));

  Payload->SetNumberField(
      TEXT("frame"), static_cast<double>(MIN_int32) - 1.0);
  Error.Reset();
  TestFalse(TEXT("one below int32 is rejected"),
            McpSequenceIntegerValidation::ValidateSequenceIntegerFields(
                Payload, Error));

  Payload->SetNumberField(
      TEXT("frame"), static_cast<double>(MAX_int32) + 1.0);
  Error.Reset();
  TestFalse(TEXT("one above int32 is rejected"),
            McpSequenceIntegerValidation::ValidateSequenceIntegerFields(
                Payload, Error));

  Payload->SetNumberField(TEXT("frame"), 1.5);
  Error.Reset();
  TestFalse(TEXT("fractional integers are rejected"),
            McpSequenceIntegerValidation::ValidateSequenceIntegerFields(
                Payload, Error));

  Payload = MakeShared<FJsonObject>();
  TSharedPtr<FJsonObject> Settings = MakeShared<FJsonObject>();
  Settings->SetNumberField(
      TEXT("handleFrameCount"), static_cast<double>(MAX_int32) + 1.0);
  Payload->SetObjectField(TEXT("settings"), Settings);
  Error.Reset();
  TestFalse(TEXT("nested integer overflow is rejected"),
            McpSequenceIntegerValidation::ValidateSequenceIntegerFields(
                Payload, Error));
  return true;
}
#endif

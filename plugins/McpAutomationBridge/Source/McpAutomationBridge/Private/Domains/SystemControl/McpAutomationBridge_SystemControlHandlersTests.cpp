#include "Domains/SystemControl/McpAutomationBridge_SystemControlHandlersPrivate.h"

#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/AutomationTest.h"

#include "Editor/UnrealEd/Public/Editor.h"

// run_tests runs automation tests one after another in the editor and answers with each one's result. It used to
// type "automation RunTests" into the console and answer "check the Output Log", so a caller could never tell
// whether anything passed. Each test is driven the way the engine's own automation worker drives it: start it,
// tick its latent and network commands until they are done, then stop it and read what it reported.
namespace McpSystemControlHandlers {
namespace {
constexpr int32 McpMaxTestMessages = 10;

struct FMcpTestRun {
  TArray<FAutomationTestInfo> Tests;
  int32 Matched = 0;
  int32 Index = 0;
  bool bRunning = false;
  FAutomationTestBase* Current = nullptr; // the running test's registered instance, which keeps its result
  double TestStart = 0.0;
  double TimeoutSeconds = 60.0;
  int32 Passed = 0, Failed = 0, NotRun = 0;
  TArray<TSharedPtr<FJsonValue>> Rows;
};

TSharedPtr<FJsonObject> McpTestRow(const FAutomationTestInfo& Test, bool bPassed, const FString& Note) {
  TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
  Row->SetStringField(TEXT("name"), Test.GetFullTestPath());
  Row->SetBoolField(TEXT("passed"), bPassed);
  if (!Note.IsEmpty()) {
    Row->SetStringField(TEXT("note"), Note);
  }
  return Row;
}

// What the test reported: its errors and warnings (the first few of each, as the log carries them) and their totals.
void McpAddTestEvents(const FAutomationTestExecutionInfo& Info, double Seconds, const TSharedPtr<FJsonObject>& Row) {
  Row->SetNumberField(TEXT("durationSeconds"), FMath::RoundToDouble(Seconds * 1000.0) / 1000.0);
  TArray<TSharedPtr<FJsonValue>> Errors, Warnings;
  for (const FAutomationExecutionEntry& Entry : Info.GetEntries()) {
    TArray<TSharedPtr<FJsonValue>>* List = Entry.Event.Type == EAutomationEventType::Error     ? &Errors
                                           : Entry.Event.Type == EAutomationEventType::Warning ? &Warnings
                                                                                               : nullptr;
    if (List && List->Num() < McpMaxTestMessages) {
      List->Add(MakeShared<FJsonValueString>(Entry.Event.Message.Left(300)));
    }
  }
  if (Info.GetErrorTotal() > 0) {
    Row->SetNumberField(TEXT("errorCount"), Info.GetErrorTotal());
    Row->SetArrayField(TEXT("errors"), Errors);
  }
  if (Info.GetWarningTotal() > 0) {
    Row->SetNumberField(TEXT("warningCount"), Info.GetWarningTotal());
    Row->SetArrayField(TEXT("warnings"), Warnings);
  }
}

void McpSendTestRun(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, FSystemControlSocket Socket,
                    const FMcpTestRun& Run) {
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetArrayField(TEXT("tests"), Run.Rows);
  Result->SetNumberField(TEXT("matched"), Run.Matched);
  Result->SetNumberField(TEXT("passed"), Run.Passed);
  Result->SetNumberField(TEXT("failed"), Run.Failed);
  if (Run.NotRun > 0) {
    Result->SetNumberField(TEXT("notRun"), Run.NotRun);
  }
  Result->SetBoolField(TEXT("allPassed"), Run.Failed == 0 && Run.NotRun == 0 && Run.Passed > 0);
  FString Message = FString::Printf(TEXT("Ran %d of %d matching test(s): %d passed, %d failed"), Run.Passed + Run.Failed,
                                    Run.Matched, Run.Passed, Run.Failed);
  if (Run.NotRun > 0) {
    Message += FString::Printf(TEXT(", %d not run"), Run.NotRun);
  }
  // A failed test is a finding, not a failed call: tests[] says which one and why.
  Self->SendAutomationResponse(Socket, RequestId, true, Message, Result, FString());
}

// One tick of the run: finish the test in progress when its commands are done (or it timed out), else start the
// next one. Answers and returns false once every test has had its turn.
bool McpTickTestRun(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, FSystemControlSocket Socket,
                    FMcpTestRun& Run) {
  FAutomationTestFramework& Framework = FAutomationTestFramework::Get();
  if (Run.bRunning) {
    const double Seconds = FPlatformTime::Seconds() - Run.TestStart;
    const bool bTimedOut = Seconds > Run.TimeoutSeconds;
    // The editor's own automation worker ticks every frame too, and finishes a test it finds running whenever it
    // expects the next network command (always true for its first one): then the framework is no longer testing,
    // and the result is read off the test instead. ExecuteLatentCommands runs a test's latent commands, which
    // that worker also runs once a frame.
    // ponytail: latent commands can run twice a frame; a test counting frames would see them pass at double speed.
    if (GIsAutomationTesting && Framework.GetCurrentTest() == Run.Current) {
      if (!bTimedOut && !(Framework.ExecuteLatentCommands() && Framework.ExecuteNetworkCommands())) {
        return true;
      }
      if (bTimedOut) {
        Framework.DequeueAllCommands();
      } else {
        Framework.ExecuteLatentCommands(); // what the last network command queued, as the worker does
      }
      FAutomationTestExecutionInfo Stopped;
      Framework.StopTest(Stopped);
    }
    FAutomationTestExecutionInfo Info;
    Run.Current->GetExecutionInfo(Info);
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0
    const bool bPassed = Run.Current->GetSuccessState() && !bTimedOut;
#else
    const bool bPassed = Run.Current->GetLastExecutionSuccessState() && !bTimedOut;
#endif
    const TSharedPtr<FJsonObject> Row = McpTestRow(
        Run.Tests[Run.Index], bPassed,
        bTimedOut ? FString::Printf(TEXT("Stopped after %.0f s (timeoutSeconds)."), Run.TimeoutSeconds) : FString());
    McpAddTestEvents(Info, Seconds, Row);
    Run.Rows.Add(MakeShared<FJsonValueObject>(Row));
    (bPassed ? Run.Passed : Run.Failed) += 1;
    Run.bRunning = false;
    Run.Index += 1;
    return true;
  }
  if (Run.Index >= Run.Tests.Num()) {
    McpSendTestRun(Self, RequestId, Socket, Run);
    return false;
  }
  const FAutomationTestInfo& Test = Run.Tests[Run.Index];
  // Another runner (the Session Frontend) or a game started meanwhile: the framework would refuse or interleave.
  const bool bBlocked = GIsAutomationTesting || (GEditor && GEditor->PlayWorld);
  if (!bBlocked) {
    Self->SendProgressUpdate(RequestId, 100.0f * Run.Index / Run.Tests.Num(),
                             FString::Printf(TEXT("Running test %d of %d: %s"), Run.Index + 1, Run.Tests.Num(),
                                             *Test.GetFullTestPath()));
    Framework.StartTestByName(Test.GetTestName(), 0);
    Run.Current = GIsAutomationTesting ? Framework.GetCurrentTest() : nullptr;
    if (Run.Current) {
      Run.bRunning = true;
      Run.TestStart = FPlatformTime::Seconds();
      return true;
    }
  }
  Run.Rows.Add(MakeShared<FJsonValueObject>(McpTestRow(
      Test, false,
      bBlocked ? TEXT("Not run: another automation run or Play In Editor started meanwhile.")
               : TEXT("Not run: the automation framework did not start it (see the Output Log)."))));
  Run.NotRun += 1;
  Run.Index += 1;
  return true;
}
} // namespace

bool HandleRunTests(UMcpAutomationBridgeSubsystem* Self,
                    const FString& RequestId,
                    const TSharedPtr<FJsonObject>& Payload,
                    FSystemControlSocket RequestingSocket) {
  FString Filter;
  Payload->TryGetStringField(TEXT("filter"), Filter);
  Filter.TrimStartAndEndInline();
  if (!McpIsSafeAutomationTestFilter(Filter)) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Test filter contains unsafe characters"),
                              TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (GEditor && GEditor->PlayWorld) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Play In Editor is running, and the automation framework starts no test while "
                                   "a game runs: stop it (control_editor.stop) first."),
                              TEXT("PIE_RUNNING"));
    return true;
  }
  if (GIsAutomationTesting) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Another automation run is in progress (the Session Frontend or an earlier "
                                   "run_tests); run again once it ends."),
                              TEXT("AUTOMATION_BUSY"));
    return true;
  }

  double MaxTests = 50.0, TimeoutSeconds = 60.0;
  Payload->TryGetNumberField(TEXT("maxTests"), MaxTests);
  Payload->TryGetNumberField(TEXT("timeoutSeconds"), TimeoutSeconds);
  TSharedRef<FMcpTestRun> Run = MakeShared<FMcpTestRun>();
  Run->TimeoutSeconds = FMath::Clamp(TimeoutSeconds, 1.0, 600.0);
  const int32 Cap = FMath::Clamp(FMath::RoundToInt(MaxTests), 1, 200);

  // Every test this editor can run, whatever filter an earlier run left on the framework.
  FAutomationTestFramework& Framework = FAutomationTestFramework::Get();
  // Spelled out: the filter mask is EAutomationTestFlags::FilterMask before 5.5 and EAutomationTestFlags_FilterMask after.
  Framework.SetRequestedTestFilter(EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter |
                                   EAutomationTestFlags::EngineFilter | EAutomationTestFlags::ProductFilter |
                                   EAutomationTestFlags::PerfFilter | EAutomationTestFlags::StressFilter |
                                   EAutomationTestFlags::NegativeFilter);
  TArray<FAutomationTestInfo> Available;
  Framework.GetValidTestNames(Available);
  for (const FAutomationTestInfo& Test : Available) {
    if (Filter.IsEmpty() || Test.GetFullTestPath().Contains(Filter, ESearchCase::IgnoreCase)) {
      Run->Matched += 1;
      if (Run->Tests.Num() < Cap) {
        Run->Tests.Add(Test);
      }
    }
  }
  if (Run->Matched == 0) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              FString::Printf(TEXT("No automation test's full path contains '%s' (%d tests are "
                                                   "available, e.g. System.Core, Project.Functional Tests)."),
                                              *Filter, Available.Num()),
                              TEXT("NOT_FOUND"));
    return true;
  }

  TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSelf(Self);
  FTSTicker::GetCoreTicker().AddTicker(
      FTickerDelegate::CreateLambda([WeakSelf, Run, RequestId, RequestingSocket](float) -> bool {
        UMcpAutomationBridgeSubsystem* Subsystem = WeakSelf.Get();
        return Subsystem && McpTickTestRun(Subsystem, RequestId, RequestingSocket, *Run);
      }),
      0.0f);
  return true;
}
}

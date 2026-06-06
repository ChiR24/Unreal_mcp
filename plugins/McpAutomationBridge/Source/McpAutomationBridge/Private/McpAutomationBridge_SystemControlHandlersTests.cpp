#include "McpAutomationBridge_SystemControlHandlersPrivate.h"

#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "HAL/PlatformProcess.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Editor.h"
#endif

namespace McpSystemControlHandlers {

bool HandleRunTests(UMcpAutomationBridgeSubsystem* Self,
                    const FString& RequestId,
                    const TSharedPtr<FJsonObject>& Payload,
                    FSystemControlSocket RequestingSocket) {
#if WITH_EDITOR
  FString Filter;
  Payload->TryGetStringField(TEXT("filter"), Filter);

  FString TestName;
  Payload->TryGetStringField(TEXT("test"), TestName);

  if (!TestName.IsEmpty() && Filter.IsEmpty()) {
    Filter = TestName;
  }
  Filter.TrimStartAndEndInline();
  if (!McpIsSafeAutomationTestFilter(Filter)) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Test filter contains unsafe characters"),
                              TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString TestCommand;
  if (Filter.IsEmpty()) {
    TestCommand = TEXT("automation RunAll");
  } else {
    TestCommand = FString::Printf(TEXT("automation RunTests %s"), *Filter);
  }

  if (GEngine && GEditor && GEditor->GetEditorWorldContext().World()) {
    GEngine->Exec(GEditor->GetEditorWorldContext().World(), *TestCommand);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("command"), TestCommand);
    Result->SetStringField(TEXT("filter"), Filter);

    Self->SendAutomationResponse(
        RequestingSocket, RequestId, true,
        TEXT("Automation tests started. Check Output Log for results."),
        Result);
  } else {
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Editor world not available for running tests"),
        TEXT("EDITOR_NOT_AVAILABLE"));
  }
  return true;
#else
  return false;
#endif
}

bool HandleTestProgressProtocol(UMcpAutomationBridgeSubsystem* Self,
                                const FString& RequestId,
                                const TSharedPtr<FJsonObject>& Payload,
                                FSystemControlSocket RequestingSocket) {
#if WITH_EDITOR
  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("test_progress_protocol: Handler called successfully"));
  int32 Steps = 5;
  Payload->TryGetNumberField(TEXT("steps"), Steps);
  Steps = FMath::Clamp(Steps, 1, 20);

  float StepDurationMs = 500.0f;
  Payload->TryGetNumberField(TEXT("stepDurationMs"), StepDurationMs);
  StepDurationMs = FMath::Clamp(StepDurationMs, 100.0f, 5000.0f);

  bool bSendProgress = true;
  if (Payload->HasField(TEXT("sendProgress"))) {
    bSendProgress = Payload->GetBoolField(TEXT("sendProgress"));
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("test_progress_protocol: Starting %d steps, %.0fms each, progress=%s"),
         Steps, StepDurationMs, bSendProgress ? TEXT("true") : TEXT("false"));

  for (int32 i = 1; i <= Steps; i++) {
    if (bSendProgress) {
      float Percent =
          (static_cast<float>(i) / static_cast<float>(Steps)) * 100.0f;
      FString StatusMsg = FString::Printf(TEXT("Step %d/%d"), i, Steps);
      Self->SendProgressUpdate(RequestId, Percent, StatusMsg, true);
    }

    FPlatformProcess::Sleep(StepDurationMs / 1000.0f);
  }

  if (bSendProgress) {
    Self->SendProgressUpdate(RequestId, 100.0f, TEXT("Complete"), false);
  }

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetNumberField(TEXT("steps"), Steps);
  Result->SetNumberField(TEXT("stepDurationMs"), StepDurationMs);
  Result->SetBoolField(TEXT("progressSent"), bSendProgress);
  Result->SetStringField(TEXT("message"),
                         TEXT("Progress protocol test completed"));

  Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Progress protocol test completed successfully"),
                               Result);
  return true;
#else
  return false;
#endif
}

bool HandleTestStaleProgress(UMcpAutomationBridgeSubsystem* Self,
                             const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload,
                             FSystemControlSocket RequestingSocket) {
#if WITH_EDITOR
  int32 StaleCount = 5;
  Payload->TryGetNumberField(TEXT("staleCount"), StaleCount);
  StaleCount = FMath::Clamp(StaleCount, 1, 10);

  UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
         TEXT("test_stale_progress: Sending %d stale updates"), StaleCount);

  for (int32 i = 0; i < StaleCount; i++) {
    FString StatusMsg =
        FString::Printf(TEXT("Stale update %d/%d"), i + 1, StaleCount);
    Self->SendProgressUpdate(RequestId, 50.0f, StatusMsg, true);
    FPlatformProcess::Sleep(0.1f);
  }

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetNumberField(TEXT("staleUpdatesSent"), StaleCount);
  Result->SetBoolField(TEXT("staleDetectionExpected"), true);

  Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                               TEXT("Stale progress test completed"), Result);
  return true;
#else
  return false;
#endif
}

}

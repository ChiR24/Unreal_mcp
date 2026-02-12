#include "McpAutomationBridgeGlobals.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"

#if WITH_EDITOR
#include "Editor/UnrealEd/Public/Editor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "Misc/MonitoredProcess.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleSystemControlAction(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  // The sub-action is in the payload's "action" field
  FString SubAction;
  if (Payload.IsValid()) {
    Payload->TryGetStringField(TEXT("action"), SubAction);
  }
  
  const FString Lower = SubAction.ToLower();
  
  // Check if this handler should process this sub-action
  if (!Lower.StartsWith(TEXT("run_ubt")) &&
      !Lower.StartsWith(TEXT("run_tests")) &&
      !Lower.StartsWith(TEXT("test_progress")) &&
      !Lower.StartsWith(TEXT("test_stale"))) {
    return false; // Not handled by this function
  }

#if WITH_EDITOR
  if (!Payload.IsValid()) {
    SendAutomationError(RequestingSocket, RequestId,
                        TEXT("System control payload missing"),
                        TEXT("INVALID_PAYLOAD"));
    return true;
  }

  if (Lower == TEXT("run_ubt")) {
    // Extract optional parameters
    FString Target;
    Payload->TryGetStringField(TEXT("target"), Target);
    
    FString Platform;
    Payload->TryGetStringField(TEXT("platform"), Platform);
    
    FString Configuration;
    Payload->TryGetStringField(TEXT("configuration"), Configuration);
    
    FString AdditionalArgs;
    Payload->TryGetStringField(TEXT("additionalArgs"), AdditionalArgs);

    // Build UBT path
    FString EngineDir = FPaths::EngineDir();
    FString UBTPath;
    
#if PLATFORM_WINDOWS
    UBTPath = FPaths::Combine(EngineDir, TEXT("Build/BatchFiles/Build.bat"));
#else
    UBTPath = FPaths::Combine(EngineDir, TEXT("Build/BatchFiles/Build.sh"));
#endif

    if (!FPaths::FileExists(UBTPath)) {
      SendAutomationError(RequestingSocket, RequestId,
                          FString::Printf(TEXT("UBT not found at: %s"), *UBTPath),
                          TEXT("UBT_NOT_FOUND"));
      return true;
    }

    // Build command line arguments
    FString Arguments;
    
    // Target (project or engine target)
    if (!Target.IsEmpty()) {
      Arguments += Target + TEXT(" ");
    } else {
      // Default to current project
      FString ProjectPath = FPaths::GetProjectFilePath();
      if (!ProjectPath.IsEmpty()) {
        Arguments += FString::Printf(TEXT("-project=\"%s\" "), *ProjectPath);
      }
    }
    
    // Platform
    if (!Platform.IsEmpty()) {
      Arguments += Platform + TEXT(" ");
    } else {
#if PLATFORM_WINDOWS
      Arguments += TEXT("Win64 ");
#elif PLATFORM_MAC
      Arguments += TEXT("Mac ");
#else
      Arguments += TEXT("Linux ");
#endif
    }
    
    // Configuration
    if (!Configuration.IsEmpty()) {
      Arguments += Configuration + TEXT(" ");
    } else {
      Arguments += TEXT("Development ");
    }
    
    // Additional args
    if (!AdditionalArgs.IsEmpty()) {
      Arguments += AdditionalArgs;
    }

    // Use FMonitoredProcess for non-blocking execution with output capture
    // For simplicity, we'll use a synchronous approach with timeout
    int32 ReturnCode = -1;
    FString StdOut;
    FString StdErr;
    
    // Note: FPlatformProcess::ExecProcess is simpler but blocks
    // Using CreateProc with pipes for better control
    void* ReadPipe = nullptr;
    void* WritePipe = nullptr;
    FPlatformProcess::CreatePipe(ReadPipe, WritePipe);
    
    FProcHandle ProcessHandle = FPlatformProcess::CreateProc(
        *UBTPath,
        *Arguments,
        false,  // bLaunchDetached
        true,   // bLaunchHidden
        true,   // bLaunchReallyHidden
        nullptr, // OutProcessID
        0,      // PriorityModifier
        nullptr, // OptionalWorkingDirectory
        WritePipe // PipeWriteChild
    );

    if (!ProcessHandle.IsValid()) {
      FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Failed to launch UBT process"),
                          TEXT("PROCESS_LAUNCH_FAILED"));
      return true;
    }

    // Read output with timeout (30 seconds max wait, but check periodically)
    const double TimeoutSeconds = 300.0; // 5 minute timeout for builds
    const double StartTime = FPlatformTime::Seconds();
    
    while (FPlatformProcess::IsProcRunning(ProcessHandle)) {
      // Read available output
      FString NewOutput = FPlatformProcess::ReadPipe(ReadPipe);
      if (!NewOutput.IsEmpty()) {
        StdOut += NewOutput;
      }
      
      // Check timeout
      if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds) {
        FPlatformProcess::TerminateProc(ProcessHandle, true);
        FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
        
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("output"), StdOut);
        Result->SetBoolField(TEXT("timedOut"), true);
        
        SendAutomationResponse(RequestingSocket, RequestId, false,
                               TEXT("UBT process timed out"), Result,
                               TEXT("TIMEOUT"));
        return true;
      }
      
      // Small sleep to avoid busy waiting
      FPlatformProcess::Sleep(0.1f);
    }

    // Read any remaining output
    FString FinalOutput = FPlatformProcess::ReadPipe(ReadPipe);
    if (!FinalOutput.IsEmpty()) {
      StdOut += FinalOutput;
    }

    // Get return code
    FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode);
    FPlatformProcess::CloseProc(ProcessHandle);
    FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("output"), StdOut);
    Result->SetNumberField(TEXT("returnCode"), ReturnCode);
    Result->SetStringField(TEXT("ubtPath"), UBTPath);
    Result->SetStringField(TEXT("arguments"), Arguments);

    if (ReturnCode == 0) {
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("UBT completed successfully"), Result);
    } else {
      SendAutomationResponse(RequestingSocket, RequestId, false,
                             FString::Printf(TEXT("UBT failed with code %d"), ReturnCode),
                             Result, TEXT("UBT_FAILED"));
    }
    return true;
  } else if (Lower == TEXT("run_tests")) {
    // Extract test filter
    FString Filter;
    Payload->TryGetStringField(TEXT("filter"), Filter);
    
    FString TestName;
    Payload->TryGetStringField(TEXT("test"), TestName);
    
    // If specific test name provided, use it as filter
    if (!TestName.IsEmpty() && Filter.IsEmpty()) {
      Filter = TestName;
    }
    
    // Build automation test command
    FString TestCommand;
    if (Filter.IsEmpty()) {
      // Run all tests
      TestCommand = TEXT("automation RunAll");
    } else {
      // Run filtered tests
      TestCommand = FString::Printf(TEXT("automation RunTests %s"), *Filter);
    }

    // Execute the automation command
    if (GEngine && GEditor && GEditor->GetEditorWorldContext().World()) {
      GEngine->Exec(GEditor->GetEditorWorldContext().World(), *TestCommand);
      
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("command"), TestCommand);
      Result->SetStringField(TEXT("filter"), Filter);
      
      // Note: Automation tests run asynchronously in UE.
      // The command starts the tests, but results come later via automation framework.
      SendAutomationResponse(RequestingSocket, RequestId, true,
                             TEXT("Automation tests started. Check Output Log for results."),
                             Result);
    } else {
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Editor world not available for running tests"),
                          TEXT("EDITOR_NOT_AVAILABLE"));
    }
    return true;
  } else   if (Lower == TEXT("test_progress_protocol")) {
    // Test action for heartbeat/progress protocol
    // Simulates a long-running operation with progress updates
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
      // Send progress update before each step
      if (bSendProgress) {
        float Percent = (static_cast<float>(i) / static_cast<float>(Steps)) * 100.0f;
        FString StatusMsg = FString::Printf(TEXT("Step %d/%d"), i, Steps);
        SendProgressUpdate(RequestId, Percent, StatusMsg, true);
      }
      
      // Simulate work by sleeping
      FPlatformProcess::Sleep(StepDurationMs / 1000.0f);
    }
    
    // Send final progress indicating completion
    if (bSendProgress) {
      SendProgressUpdate(RequestId, 100.0f, TEXT("Complete"), false);
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("steps"), Steps);
    Result->SetNumberField(TEXT("stepDurationMs"), StepDurationMs);
    Result->SetBoolField(TEXT("progressSent"), bSendProgress);
    Result->SetStringField(TEXT("message"), TEXT("Progress protocol test completed"));
    
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Progress protocol test completed successfully"), Result);
    return true;
  } else if (Lower == TEXT("test_stale_progress")) {
    // Test action for stale progress detection
    // Sends the same progress percent multiple times to trigger stale detection
    int32 StaleCount = 5;
    Payload->TryGetNumberField(TEXT("staleCount"), StaleCount);
    StaleCount = FMath::Clamp(StaleCount, 1, 10);
    
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log,
           TEXT("test_stale_progress: Sending %d stale updates"), StaleCount);
    
    // Send same progress repeatedly to trigger stale detection
    for (int32 i = 0; i < StaleCount; i++) {
      FString StatusMsg = FString::Printf(TEXT("Stale update %d/%d"), i + 1, StaleCount);
      SendProgressUpdate(RequestId, 50.0f, StatusMsg, true);  // Always 50%
      FPlatformProcess::Sleep(0.1f);  // 100ms between updates
    }
    
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("staleUpdatesSent"), StaleCount);
    Result->SetBoolField(TEXT("staleDetectionExpected"), true);
    
    SendAutomationResponse(RequestingSocket, RequestId, true,
                           TEXT("Stale progress test completed"), Result);
    return true;
  }

  return false;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("System control actions require editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

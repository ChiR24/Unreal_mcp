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
  const FString Lower = Action.ToLower();
  if (!Lower.StartsWith(TEXT("run_ubt")) &&
      !Lower.StartsWith(TEXT("run_tests"))) {
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
  }

  return false;
#else
  SendAutomationResponse(RequestingSocket, RequestId, false,
                         TEXT("System control actions require editor build"),
                         nullptr, TEXT("NOT_IMPLEMENTED"));
  return true;
#endif
}

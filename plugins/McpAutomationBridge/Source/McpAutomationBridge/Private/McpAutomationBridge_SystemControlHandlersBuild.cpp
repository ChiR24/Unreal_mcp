#include "McpAutomationBridge_SystemControlHandlersPrivate.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/Paths.h"

namespace McpSystemControlHandlers {

bool HandleRunUbt(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
                  const TSharedPtr<FJsonObject>& Payload,
                  FSystemControlSocket RequestingSocket) {
#if WITH_EDITOR
  FString Target;
  Payload->TryGetStringField(TEXT("target"), Target);

  FString Platform;
  Payload->TryGetStringField(TEXT("platform"), Platform);

  FString Configuration;
  Payload->TryGetStringField(TEXT("configuration"), Configuration);

  FString AdditionalArgs;
  Payload->TryGetStringField(TEXT("additionalArgs"), AdditionalArgs);
  if (AdditionalArgs.IsEmpty()) {
    Payload->TryGetStringField(TEXT("arguments"), AdditionalArgs);
  }

  Target.TrimStartAndEndInline();
  Platform.TrimStartAndEndInline();
  Configuration.TrimStartAndEndInline();
  AdditionalArgs.TrimStartAndEndInline();

  auto ValidateBuildToken = [&](const FString& Value,
                                const TCHAR* FieldName) -> bool {
    if (!McpIsSafeUbtPositionalToken(Value)) {
      Self->SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(
              TEXT("Invalid %s for run_ubt: %s must be a positional token"),
              FieldName, *Value),
          TEXT("INVALID_ARGUMENT"));
      return false;
    }
    return true;
  };

  if (Target.IsEmpty()) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Target is required for run_ubt"),
                              TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (Platform.IsEmpty()) {
#if PLATFORM_WINDOWS
    Platform = TEXT("Win64");
#elif PLATFORM_MAC
    Platform = TEXT("Mac");
#else
    Platform = TEXT("Linux");
#endif
  }

  if (Configuration.IsEmpty()) {
    Configuration = TEXT("Development");
  }

  if (!ValidateBuildToken(Target, TEXT("target")) ||
      !ValidateBuildToken(Platform, TEXT("platform")) ||
      !ValidateBuildToken(Configuration, TEXT("configuration"))) {
    return true;
  }

  if (!McpIsAllowedUbtPlatform(Platform)) {
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Platform is not allowed for run_ubt: %s"),
                        *Platform),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!McpIsAllowedUbtConfiguration(Configuration)) {
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Configuration is not allowed for run_ubt: %s"),
                        *Configuration),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  if (!McpIsSafeUbtArgumentList(AdditionalArgs)) {
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("additionalArgs contains unsafe UBT argument characters"),
        TEXT("INVALID_ARGUMENT"));
    return true;
  }

  FString UBTPath;
#if PLATFORM_WINDOWS
  UBTPath = FPaths::Combine(FPaths::EngineDir(), TEXT("Build/BatchFiles/Build.bat"));
#elif PLATFORM_MAC
  UBTPath = FPaths::Combine(FPaths::EngineDir(), TEXT("Build/BatchFiles/Mac/Build.sh"));
#else
  UBTPath = FPaths::Combine(FPaths::EngineDir(), TEXT("Build/BatchFiles/Linux/Build.sh"));
#endif

  if (!FPaths::FileExists(UBTPath)) {
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("UBT not found at: %s"), *UBTPath),
        TEXT("UBT_NOT_FOUND"));
    return true;
  }

  FString Arguments;
  Arguments += Target + TEXT(" ");
  Arguments += Platform + TEXT(" ");
  Arguments += Configuration + TEXT(" ");

  const FString ProjectPath = FPaths::GetProjectFilePath();
  if (!ProjectPath.IsEmpty()) {
    Arguments += FString::Printf(TEXT("-Project=\"%s\" "), *ProjectPath);
  }
  if (!AdditionalArgs.IsEmpty()) {
    Arguments += AdditionalArgs;
  }

  int32 ReturnCode = -1;
  FString StdOut;
  void* ReadPipe = nullptr;
  void* WritePipe = nullptr;
  FPlatformProcess::CreatePipe(ReadPipe, WritePipe);

  FProcHandle ProcessHandle = FPlatformProcess::CreateProc(
      *UBTPath, *Arguments, false, true, true, nullptr, 0, nullptr, WritePipe);

  if (!ProcessHandle.IsValid()) {
    FPlatformProcess::ClosePipe(ReadPipe, WritePipe);
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Failed to launch UBT process"),
                              TEXT("PROCESS_LAUNCH_FAILED"));
    return true;
  }

  const double TimeoutSeconds = 300.0;
  const double StartTime = FPlatformTime::Seconds();

  while (FPlatformProcess::IsProcRunning(ProcessHandle)) {
    FString NewOutput = FPlatformProcess::ReadPipe(ReadPipe);
    if (!NewOutput.IsEmpty()) {
      StdOut += NewOutput;
    }

    if (FPlatformTime::Seconds() - StartTime > TimeoutSeconds) {
      FPlatformProcess::TerminateProc(ProcessHandle, true);
      FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("output"), StdOut);
      Result->SetBoolField(TEXT("timedOut"), true);

      Self->SendAutomationResponse(RequestingSocket, RequestId, false,
                                   TEXT("UBT process timed out"), Result,
                                   TEXT("TIMEOUT"));
      return true;
    }

    FPlatformProcess::Sleep(0.1f);
  }

  FString FinalOutput = FPlatformProcess::ReadPipe(ReadPipe);
  if (!FinalOutput.IsEmpty()) {
    StdOut += FinalOutput;
  }

  FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode);
  FPlatformProcess::CloseProc(ProcessHandle);
  FPlatformProcess::ClosePipe(ReadPipe, WritePipe);

  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetStringField(TEXT("output"), StdOut);
  Result->SetNumberField(TEXT("returnCode"), ReturnCode);
  Result->SetStringField(TEXT("ubtPath"), UBTPath);
  Result->SetStringField(TEXT("arguments"), Arguments);

  if (ReturnCode == 0) {
    Self->SendAutomationResponse(RequestingSocket, RequestId, true,
                                 TEXT("UBT completed successfully"), Result);
  } else {
    Self->SendAutomationResponse(
        RequestingSocket, RequestId, false,
        FString::Printf(TEXT("UBT failed with code %d"), ReturnCode), Result,
        TEXT("UBT_FAILED"));
  }
  return true;
#else
  return false;
#endif
}

}

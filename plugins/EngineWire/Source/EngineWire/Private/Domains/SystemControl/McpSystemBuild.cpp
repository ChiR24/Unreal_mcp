#include "Domains/SystemControl/McpSystemPrivate.h"

#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "EngineWireSubsystem.h"
#include "Misc/Paths.h"

namespace McpSystemControlHandlers {

bool HandleRunUbt(UEngineWireSubsystem* Self, const FString& RequestId,
                  const TSharedPtr<FJsonObject>& Payload,
                  FSystemControlSocket RequestingSocket) {
  FString Target;
  Payload->TryGetStringField(TEXT("target"), Target);

  FString Platform;
  Payload->TryGetStringField(TEXT("platform"), Platform);

  FString Configuration;
  Payload->TryGetStringField(TEXT("configuration"), Configuration);

  FString AdditionalArgs;
  Payload->TryGetStringField(TEXT("arguments"), AdditionalArgs);

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

  // The build is polled from the core ticker and answered when it ends: waiting for it here, on the game thread, stopped
  // the editor ticking, painting and draining its request queue for the whole build.
  struct FUbtRun {
    TWeakObjectPtr<UEngineWireSubsystem> Self;
    FString RequestId, UBTPath, Arguments, StdOut;
    FSystemControlSocket Socket;
    FProcHandle Proc;
    void* ReadPipe = nullptr;
    void* WritePipe = nullptr;
    double StartTime = 0.0;
  };
  const TSharedRef<FUbtRun> Run = MakeShared<FUbtRun>();
  Run->Self = Self;
  Run->RequestId = RequestId;
  Run->UBTPath = UBTPath;
  Run->Arguments = Arguments;
  Run->Socket = RequestingSocket;
  Run->Proc = ProcessHandle;
  Run->ReadPipe = ReadPipe;
  Run->WritePipe = WritePipe;
  Run->StartTime = FPlatformTime::Seconds();
  FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float) {
    constexpr double TimeoutSeconds = 300.0;
    Run->StdOut += FPlatformProcess::ReadPipe(Run->ReadPipe);
    const bool bRunning = FPlatformProcess::IsProcRunning(Run->Proc);
    const bool bTimedOut = bRunning && FPlatformTime::Seconds() - Run->StartTime > TimeoutSeconds;
    if (bRunning && !bTimedOut) {
      return true;
    }
    int32 ReturnCode = -1;
    if (bTimedOut) {
      FPlatformProcess::TerminateProc(Run->Proc, true);
    } else {
      Run->StdOut += FPlatformProcess::ReadPipe(Run->ReadPipe);
      FPlatformProcess::GetProcReturnCode(Run->Proc, &ReturnCode);
    }
    FPlatformProcess::CloseProc(Run->Proc);
    FPlatformProcess::ClosePipe(Run->ReadPipe, Run->WritePipe);
    UEngineWireSubsystem* Subsystem = Run->Self.Get();
    if (!Subsystem) {
      return false;
    }
    Subsystem->RunLatentReply(Run->RequestId, TEXT("system_control"), [&]() {
      TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
      Result->SetStringField(TEXT("output"), Run->StdOut);
      if (bTimedOut) {
        Result->SetBoolField(TEXT("timedOut"), true);
        Subsystem->SendAutomationResponse(Run->Socket, Run->RequestId, false, TEXT("UBT process timed out"), Result, TEXT("TIMEOUT"));
        return;
      }
      Result->SetNumberField(TEXT("returnCode"), ReturnCode);
      Result->SetStringField(TEXT("ubtPath"), Run->UBTPath);
      Result->SetStringField(TEXT("arguments"), Run->Arguments);
      if (ReturnCode == 0) {
        Subsystem->SendAutomationResponse(Run->Socket, Run->RequestId, true, TEXT("UBT completed successfully"), Result);
      } else {
        Subsystem->SendAutomationResponse(Run->Socket, Run->RequestId, false,
                                          FString::Printf(TEXT("UBT failed with code %d"), ReturnCode), Result, TEXT("UBT_FAILED"));
      }
    });
    return false;
  }), 0.1f);
  return true;
}

}

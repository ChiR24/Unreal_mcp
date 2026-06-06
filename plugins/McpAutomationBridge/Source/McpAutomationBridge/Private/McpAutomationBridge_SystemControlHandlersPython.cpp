#include "McpAutomationBridge_SystemControlHandlersPrivate.h"
#include "McpAutomationBridge_SystemControlHandlersPythonExecution.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformTime.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_EDITOR
#include "IPythonScriptPlugin.h"
#include "Modules/ModuleManager.h"
#endif

namespace McpSystemControlHandlers {

bool HandleExecutePython(UMcpAutomationBridgeSubsystem* Self,
                         const FString& RequestId,
                         const TSharedPtr<FJsonObject>& Payload,
                         FSystemControlSocket RequestingSocket) {
#if WITH_EDITOR
  FString Code;
  Payload->TryGetStringField(TEXT("code"), Code);
  FString File;
  Payload->TryGetStringField(TEXT("file"), File);

  const bool bHasCode = !Code.TrimStartAndEnd().IsEmpty();
  const bool bHasFile = !File.TrimStartAndEnd().IsEmpty();

  if (!bHasCode && !bHasFile) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("'code' or 'file' parameter is required"),
                              TEXT("INVALID_ARGUMENT"));
    return true;
  }
  if (bHasCode && bHasFile) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Provide either 'code' or 'file', not both"),
                              TEXT("INVALID_ARGUMENT"));
    return true;
  }

  static const int32 MaxCodeSize = 1048576;
  if (Code.Len() > MaxCodeSize) {
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Python code exceeds maximum size (%d bytes)"),
                        MaxCodeSize),
        TEXT("CODE_TOO_LARGE"));
    return true;
  }

  FString TempDir = FPaths::ProjectSavedDir() / TEXT("Temp/MCP_Python");
  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  if (!PlatformFile.DirectoryExists(*TempDir)) {
    PlatformFile.CreateDirectoryTree(*TempDir);
  }

  FString SafeId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
  FString ScriptPath = TempDir / FString::Printf(TEXT("mcp_exec_%s.py"), *SafeId);
  FString OutputPath = TempDir / FString::Printf(TEXT("output_%s.txt"), *SafeId);
  FString ErrorPath = TempDir / FString::Printf(TEXT("error_%s.txt"), *SafeId);
  FString StatusPath = TempDir / FString::Printf(TEXT("status_%s.txt"), *SafeId);
  FString CodePath = TempDir / FString::Printf(TEXT("code_%s.py"), *SafeId);

  FPythonTempFileCleanup Cleanup;
  Cleanup.Paths.Add(ScriptPath);
  Cleanup.Paths.Add(OutputPath);
  Cleanup.Paths.Add(ErrorPath);
  Cleanup.Paths.Add(StatusPath);
  Cleanup.Paths.Add(CodePath);

  FString PyOutputPath = NormalizePythonPath(OutputPath);
  FString PyErrorPath = NormalizePythonPath(ErrorPath);
  FString PyStatusPath = NormalizePythonPath(StatusPath);

  FString Wrapper;
  Wrapper += TEXT("import sys\nimport traceback\n\n");
  Wrapper += FString::Printf(TEXT("_out = open(r'%s', 'w', encoding='utf-8')\n"),
                             *PyOutputPath);
  Wrapper += FString::Printf(TEXT("_err = open(r'%s', 'w', encoding='utf-8')\n"),
                             *PyErrorPath);
  Wrapper += TEXT("_old_out, _old_err = sys.stdout, sys.stderr\n");
  Wrapper += TEXT("sys.stdout, sys.stderr = _out, _err\n\n");
  Wrapper += TEXT("_success = True\n");
  Wrapper += TEXT("try:\n");

  if (bHasCode) {
    if (!FFileHelper::SaveStringToFile(
            Code, *CodePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
      Self->SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Failed to write temp code file: %s"), *CodePath),
          TEXT("FILE_WRITE_FAILED"));
      return true;
    }
    FString PyCodePath = NormalizePythonPath(CodePath);
    FString PyCodeDir = NormalizePythonPath(FPaths::GetPath(CodePath));
    Wrapper += FString::Printf(TEXT("    with open(r'%s', 'r', encoding='utf-8') as _f:\n"),
                               *PyCodePath);
    Wrapper += TEXT("        _user_code = _f.read()\n");
    AppendPythonExec(Wrapper, PyCodePath, PyCodeDir);
  } else {
    FString SafeFilePath = SanitizeProjectFilePath(File);
    if (SafeFilePath.IsEmpty()) {
      Self->SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Invalid or unsafe file path: %s"), *File),
          TEXT("SECURITY_VIOLATION"));
      return true;
    }

    FString AbsoluteFilePath =
        FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / SafeFilePath);
    FPaths::NormalizeFilename(AbsoluteFilePath);

    FString NormalizedProjectDir =
        FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(NormalizedProjectDir);
    if (!NormalizedProjectDir.EndsWith(TEXT("/"))) {
      NormalizedProjectDir += TEXT("/");
    }

    if (!AbsoluteFilePath.StartsWith(NormalizedProjectDir,
                                     ESearchCase::IgnoreCase)) {
      Self->SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("File path escapes project directory: %s"), *File),
          TEXT("SECURITY_VIOLATION"));
      return true;
    }

    FString ResolvedPath =
        FPlatformFileManager::Get()
            .GetPlatformFile()
            .ConvertToAbsolutePathForExternalAppForRead(*AbsoluteFilePath);
    if (!ResolvedPath.IsEmpty()) {
      FPaths::NormalizeFilename(ResolvedPath);
      if (!ResolvedPath.StartsWith(NormalizedProjectDir,
                                   ESearchCase::IgnoreCase)) {
        Self->SendAutomationError(
            RequestingSocket, RequestId,
            TEXT("Resolved file path escapes project directory (symlink detected)"),
            TEXT("SECURITY_VIOLATION"));
        return true;
      }
      AbsoluteFilePath = ResolvedPath;
    }

    FString PyFilePath = NormalizePythonPath(AbsoluteFilePath);
    FString PyFileDir = NormalizePythonPath(FPaths::GetPath(AbsoluteFilePath));
    Wrapper += FString::Printf(TEXT("    with open(r'%s', 'r', encoding='utf-8') as _f:\n"),
                               *PyFilePath);
    Wrapper += TEXT("        _user_code = _f.read()\n");
    AppendPythonExec(Wrapper, PyFilePath, PyFileDir);
  }

  Wrapper += TEXT("except:\n");
  Wrapper += TEXT("    traceback.print_exc()\n");
  Wrapper += TEXT("    _success = False\n");
  Wrapper += TEXT("finally:\n");
  Wrapper += TEXT("    sys.stdout, sys.stderr = _old_out, _old_err\n");
  Wrapper += TEXT("    _out.close()\n");
  Wrapper += TEXT("    _err.close()\n");
  Wrapper += FString::Printf(TEXT("    with open(r'%s', 'w') as _sf:\n"),
                             *PyStatusPath);
  Wrapper += TEXT("        _sf.write('1' if _success else '0')\n");

  if (!FFileHelper::SaveStringToFile(
          Wrapper, *ScriptPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        FString::Printf(TEXT("Failed to write temp script: %s"), *ScriptPath),
        TEXT("FILE_WRITE_FAILED"));
    return true;
  }

  Self->SendProgressUpdate(RequestId, 0.0f, TEXT("Executing Python script"),
                           true, Self->CurrentRequestOrigin);

  IPythonScriptPlugin* PythonPlugin =
      FModuleManager::LoadModulePtr<IPythonScriptPlugin>(TEXT("PythonScriptPlugin"));
  if (!PythonPlugin) {
    Self->SendAutomationError(RequestingSocket, RequestId,
                              TEXT("Python Editor Script Plugin module is not loaded"),
                              TEXT("PYTHON_NOT_AVAILABLE"));
    return true;
  }
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
  if (!PythonPlugin->IsPythonInitialized()) {
    PythonPlugin->ForceEnablePythonAtRuntime();
  }
  if (!PythonPlugin->IsPythonInitialized()) {
    Self->SendAutomationError(
        RequestingSocket, RequestId,
        TEXT("Python Editor Script Plugin is not initialized yet"),
        TEXT("PYTHON_NOT_AVAILABLE"));
    return true;
  }
#endif

  static constexpr double MaxPythonExecutionSeconds = 60.0;
  FPythonCommandEx PythonCommand;
  PythonCommand.Command = FString::Printf(TEXT("\"%s\""), *ScriptPath);
  PythonCommand.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
  PythonCommand.FileExecutionScope = EPythonFileExecutionScope::Private;
  PythonCommand.Flags |= EPythonCommandFlags::Unattended;
  double ExecStartTime = FPlatformTime::Seconds();
  bool bPythonCommandSucceeded = PythonPlugin->ExecPythonCommandEx(PythonCommand);
  double ExecElapsed = FPlatformTime::Seconds() - ExecStartTime;
  Self->SendProgressUpdate(RequestId, 90.0f, TEXT("Collecting Python output"),
                           true, Self->CurrentRequestOrigin);
  if (ExecElapsed > MaxPythonExecutionSeconds) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
           TEXT("Python execution took %.1fs (exceeds %.1fs threshold). "
                "Consider running long scripts via 'file' parameter in a separate process."),
           ExecElapsed, MaxPythonExecutionSeconds);
  }

  FString Output;
  FString Error;
  FString Status;
  FFileHelper::LoadFileToString(Output, *OutputPath);
  FFileHelper::LoadFileToString(Error, *ErrorPath);
  FFileHelper::LoadFileToString(Status, *StatusPath);
  if (!bPythonCommandSucceeded && Status.TrimStartAndEnd().IsEmpty()) {
    FString PythonError = PythonCommand.CommandResult;
    for (const FPythonLogOutputEntry& LogOutput : PythonCommand.LogOutput) {
      if (!PythonError.IsEmpty()) {
        PythonError += TEXT("\n");
      }
      PythonError += FString::Printf(TEXT("[%s] %s"),
                                     LexToString(LogOutput.Type),
                                     *LogOutput.Output);
    }
    if (!PythonError.IsEmpty()) {
      if (!Error.IsEmpty()) {
        Error += TEXT("\n");
      }
      Error += PythonError;
    }
  }

  bool bSuccess =
      bPythonCommandSucceeded && Status.TrimStartAndEnd().Equals(TEXT("1"));
  TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
  Result->SetStringField(TEXT("output"), Output.TrimEnd());
  Result->SetStringField(TEXT("error"), Error.TrimEnd());

  Self->SendAutomationResponse(
      RequestingSocket, RequestId, bSuccess,
      bSuccess ? TEXT("Python executed successfully")
               : TEXT("Python execution failed"),
      Result, bSuccess ? FString() : TEXT("PYTHON_ERROR"));
  return true;
#else
  return false;
#endif
}

}

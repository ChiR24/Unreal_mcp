#include "McpOpenCodeAcpClient.h"
#include "McpOpenCodeAcpClientPrivate.h"

#include "UnrealAgentEditorContext.h"
#include "UnrealAgentStudioKit.h"
#include "UnrealAgentValidationRunner.h"

#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

using namespace UnrealAgent::OpenCodeAcp;

FOpenCodeAcpClient::FOpenCodeAcpClient() = default;

FOpenCodeAcpClient::~FOpenCodeAcpClient()
{
    Stop();
}

bool FOpenCodeAcpClient::Start(const FString& InWorkingDirectory)
{
    Stop();
    ResetState();

    WorkingDirectory = FPaths::ConvertRelativePathToFull(InWorkingDirectory.IsEmpty() ? FPaths::ProjectDir() : InWorkingDirectory);
    FPaths::NormalizeDirectoryName(WorkingDirectory);

    if (!FPaths::DirectoryExists(WorkingDirectory) || !IsSafeProcessArgumentValue(WorkingDirectory))
    {
        SetStatus(FString::Printf(TEXT("OpenCode ACP working directory is invalid: %s"), *WorkingDirectory));
        return false;
    }

    if (!EnsureProjectUnrealAgentConfig())
    {
        SetStatus(FString::Printf(TEXT("Failed to prepare Unreal Agent OpenCode config in %s"), *WorkingDirectory));
        return false;
    }

    FString ResolveError;
    ResolvedExecutable = ResolveOpenCodeExecutable(ResolveError);
    if (ResolvedExecutable.IsEmpty())
    {
        SetStatus(ResolveError.IsEmpty() ? TEXT("OpenCode executable not found.") : ResolveError);
        return false;
    }

    const FString Params = TEXT("acp");

    if (!FPlatformProcess::CreatePipe(OutputReadPipe, OutputWritePipe)
        || !FPlatformProcess::CreatePipe(ErrorReadPipe, ErrorWritePipe)
        || !FPlatformProcess::CreatePipe(InputReadPipe, InputWritePipe, true))
    {
        CloseProcessPipes();
        SetStatus(TEXT("Failed to create OpenCode ACP process pipes."));
        return false;
    }

    {
        FScopedOpenCodeSignalLaunchGuard SignalLaunchGuard;
        ProcessHandle = FPlatformProcess::CreateProc(
            *ResolvedExecutable,
            *Params,
            false,
            true,
            true,
            nullptr,
            0,
            *WorkingDirectory,
            OutputWritePipe,
            InputReadPipe,
            ErrorWritePipe);
    }

    if (!ProcessHandle.IsValid())
    {
        CloseProcessPipes();
        SetStatus(FormatProcessErrorText(FString::Printf(TEXT("Failed to launch OpenCode ACP: %s %s"), *ResolvedExecutable, *Params)));
        return false;
    }

    bRunning = true;
    SetStatus(TEXT("Starting OpenCode ACP..."));
    if (!SendInitialize())
    {
        StopWithError(TEXT("Failed to send OpenCode ACP initialize request."));
        return false;
    }
    return true;
}

void FOpenCodeAcpClient::Stop()
{
    const bool bWasActive = bRunning
        || bInitialized
        || bReady
        || ProcessHandle.IsValid()
        || OutputReadPipe != nullptr
        || OutputWritePipe != nullptr
        || ErrorReadPipe != nullptr
        || ErrorWritePipe != nullptr
        || InputReadPipe != nullptr
        || InputWritePipe != nullptr;

    if (ProcessHandle.IsValid())
    {
        TerminateAndCloseProcess(ProcessHandle);
    }

    CloseProcessPipes();
    ResetState();
    if (bWasActive)
    {
        OnStopped.ExecuteIfBound();
    }
}

void FOpenCodeAcpClient::Tick()
{
    if (!bRunning)
    {
        return;
    }

    ReadProcessOutput();
    DrainProcessErrorOutput();

    if (ProcessHandle.IsValid() && !FPlatformProcess::IsProcRunning(ProcessHandle))
    {
        int32 ReturnCode = -1;
        FPlatformProcess::GetProcReturnCode(ProcessHandle, &ReturnCode);
        MarkProcessExited(ReturnCode, false);
        return;
    }

    CheckClientOwnedRequestTimeouts();
}

void FOpenCodeAcpClient::CloseProcessPipes()
{
    if (OutputReadPipe != nullptr || OutputWritePipe != nullptr)
    {
        FPlatformProcess::ClosePipe(OutputReadPipe, OutputWritePipe);
        OutputReadPipe = nullptr;
        OutputWritePipe = nullptr;
    }

    if (ErrorReadPipe != nullptr || ErrorWritePipe != nullptr)
    {
        FPlatformProcess::ClosePipe(ErrorReadPipe, ErrorWritePipe);
        ErrorReadPipe = nullptr;
        ErrorWritePipe = nullptr;
    }

    if (InputReadPipe != nullptr || InputWritePipe != nullptr)
    {
        FPlatformProcess::ClosePipe(InputReadPipe, InputWritePipe);
        InputReadPipe = nullptr;
        InputWritePipe = nullptr;
    }
}

void FOpenCodeAcpClient::MarkProcessExited(int32 ReturnCode, bool bCanceled)
{
    ReadProcessOutput();
    DrainProcessErrorOutput();
    const FString ExitText = FormatProcessErrorText(FString::Printf(TEXT("OpenCode ACP exited with code %d"), ReturnCode));

    if (ProcessHandle.IsValid())
    {
        FPlatformProcess::CloseProc(ProcessHandle);
        ProcessHandle.Reset();
    }

    CloseProcessPipes();
    ResetState();

    if (!bCanceled)
    {
        SetStatus(ExitText);
        AppendTranscript(TEXT("Error"), ExitText);
    }
    OnStopped.ExecuteIfBound();
}

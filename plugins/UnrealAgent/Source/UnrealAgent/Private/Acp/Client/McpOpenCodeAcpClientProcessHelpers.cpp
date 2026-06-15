#include "Acp/Client/McpOpenCodeAcpClientPrivate.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

#if PLATFORM_UNIX || PLATFORM_MAC
#include <limits.h>
#include <stdlib.h>
#endif

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

#if PLATFORM_UNIX || PLATFORM_MAC
#include <signal.h>
#endif

namespace UnrealAgent::OpenCodeAcp
{
bool IsSafeProcessArgumentValue(const FString& Value)
    {
        if (Value.IsEmpty())
        {
            return false;
        }

        for (const TCHAR Character : Value)
        {
            if (Character == TEXT('"') || Character == TEXT('\r') || Character == TEXT('\n') || Character < 32)
            {
                return false;
            }
        }

        return true;
    }

bool IsAbsoluteExistingExecutable(const FString& Path)
    {
        return IsSafeProcessArgumentValue(Path) && !FPaths::IsRelative(Path) && FPaths::FileExists(Path);
    }

FString ResolveExistingPath(const FString& Path)
{
#if PLATFORM_UNIX || PLATFORM_MAC
    ANSICHAR ResolvedPath[PATH_MAX];
    return realpath(TCHAR_TO_UTF8(*Path), ResolvedPath) == nullptr
        ? FString()
        : FString(UTF8_TO_TCHAR(ResolvedPath));
#elif PLATFORM_WINDOWS
    const HANDLE Handle = CreateFileW(
        *Path,
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        return FString();
    }
    const DWORD RequiredLength =
        GetFinalPathNameByHandleW(Handle, nullptr, 0, FILE_NAME_NORMALIZED);
    TArray<TCHAR> Buffer;
    Buffer.SetNumZeroed(RequiredLength + 1);
    const DWORD Written = RequiredLength > 0
        ? GetFinalPathNameByHandleW(
            Handle,
            Buffer.GetData(),
            Buffer.Num(),
            FILE_NAME_NORMALIZED)
        : 0;
    CloseHandle(Handle);
    if (Written == 0)
    {
        return FString();
    }
    FString Resolved(Buffer.GetData());
    Resolved.RemoveFromStart(TEXT("\\\\?\\"));
    return Resolved;
#else
    return Path;
#endif
}

bool IsExecutableOutsideDirectory(const FString& Path, const FString& Directory)
{
    if (!IsAbsoluteExistingExecutable(Path))
    {
        return false;
    }
    const FString ResolvedPath = ResolveExistingPath(Path);
    const FString ResolvedDirectory = ResolveExistingPath(
        FPaths::ConvertRelativePathToFull(Directory));
    return !ResolvedPath.IsEmpty()
        && !FPaths::IsSamePath(ResolvedPath, ResolvedDirectory)
        && !FPaths::IsUnderDirectory(ResolvedPath, ResolvedDirectory);
}

FString NormalizeExecutablePath(const FString& Path)
    {
        FString Normalized = Path.TrimStartAndEnd();
        FPaths::NormalizeFilename(Normalized);
        return Normalized;
    }

void TerminateAndCloseProcess(FProcHandle& ProcessHandle)
{
    if (!ProcessHandle.IsValid())
    {
        return;
    }

    if (FPlatformProcess::IsProcRunning(ProcessHandle))
    {
#if PLATFORM_UNIX
        FPlatformProcess::TerminateProc(ProcessHandle, false);
#else
        FPlatformProcess::TerminateProc(ProcessHandle, true);
#endif
        const double ShutdownDeadline = FPlatformTime::Seconds() + ClientProcessShutdownWaitSeconds;
        while (FPlatformProcess::IsProcRunning(ProcessHandle) && FPlatformTime::Seconds() < ShutdownDeadline)
        {
            FPlatformProcess::Sleep(0.01f);
        }
#if PLATFORM_UNIX || PLATFORM_MAC
        if (FPlatformProcess::IsProcRunning(ProcessHandle))
        {
            const auto ProcessId = ProcessHandle.Get();
            if (ProcessId > 0)
            {
                kill(ProcessId, SIGKILL);
            }
            const double KillDeadline =
                FPlatformTime::Seconds() + ClientProcessShutdownWaitSeconds;
            while (FPlatformProcess::IsProcRunning(ProcessHandle)
                && FPlatformTime::Seconds() < KillDeadline)
            {
                FPlatformProcess::Sleep(0.01f);
            }
        }
#endif
    }

    FPlatformProcess::CloseProc(ProcessHandle);
    ProcessHandle.Reset();
}

FScopedOpenCodeSignalLaunchGuard::FScopedOpenCodeSignalLaunchGuard()
{
#if PLATFORM_LINUX && defined(SIGPWR)
    if (sigaction(SIGPWR, nullptr, &PreviousSigPwrAction) == 0 && PreviousSigPwrAction.sa_handler == SIG_IGN)
    {
        struct sigaction DefaultAction;
        FMemory::Memzero(&DefaultAction, sizeof(DefaultAction));
        DefaultAction.sa_handler = SIG_DFL;
        sigemptyset(&DefaultAction.sa_mask);
        bRestoreSigPwr = sigaction(SIGPWR, &DefaultAction, nullptr) == 0;
    }
#endif
}

FScopedOpenCodeSignalLaunchGuard::~FScopedOpenCodeSignalLaunchGuard()
{
#if PLATFORM_LINUX && defined(SIGPWR)
    if (bRestoreSigPwr)
    {
        sigaction(SIGPWR, &PreviousSigPwrAction, nullptr);
    }
#endif
}
}

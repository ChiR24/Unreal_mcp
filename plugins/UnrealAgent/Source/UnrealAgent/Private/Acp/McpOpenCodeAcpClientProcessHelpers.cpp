#include "McpOpenCodeAcpClientPrivate.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

#if PLATFORM_LINUX
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

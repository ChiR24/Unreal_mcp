#include "Acp/Client/McpOpenCodeAcpClientPermissionPathResolution.h"

#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/Paths.h"

#if PLATFORM_UNIX || PLATFORM_MAC
#include <errno.h>
#include <sys/stat.h>
#endif

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace UnrealAgent::OpenCodeAcp::PermissionPaths
{
bool TraversesSymbolicLink(
    const FString& Candidate,
    const FString& WorkingDirectory)
{
    if (Candidate.IsEmpty() || Candidate.StartsWith(TEXT("-"))
        || Candidate.StartsWith(TEXT("/Game"))
        || Candidate.StartsWith(TEXT("/Engine")))
    {
        return false;
    }
    FString ResolvedWorkingDirectory;
    if (!ResolveExistingPath(WorkingDirectory, ResolvedWorkingDirectory))
    {
        return true;
    }
    FString FullPath = FPaths::IsRelative(Candidate)
        ? FPaths::Combine(ResolvedWorkingDirectory, Candidate)
        : Candidate;
    FPaths::NormalizeFilename(FullPath);
    if (!FPaths::IsSamePath(FullPath, ResolvedWorkingDirectory)
        && !FPaths::IsUnderDirectory(FullPath, ResolvedWorkingDirectory))
    {
        return false;
    }

    FString Relative = FullPath;
    FString RelativeBase = ResolvedWorkingDirectory;
    RelativeBase += TEXT("/");
    if (!FPaths::MakePathRelativeTo(Relative, *RelativeBase))
    {
        return true;
    }
    TArray<FString> Segments;
    Relative.ParseIntoArray(Segments, TEXT("/"), true);
    FString CurrentPath = ResolvedWorkingDirectory;
    IPlatformFile& PlatformFile =
        FPlatformFileManager::Get().GetPlatformFile();
    for (const FString& Segment : Segments)
    {
        CurrentPath = FPaths::Combine(CurrentPath, Segment);
        FPaths::NormalizeFilename(CurrentPath);
#if PLATFORM_UNIX || PLATFORM_MAC
        struct stat FileInfo;
        if (lstat(TCHAR_TO_UTF8(*CurrentPath), &FileInfo) == 0)
        {
            if (S_ISLNK(FileInfo.st_mode))
            {
                return true;
            }
        }
        else if (errno != ENOENT)
        {
            return true;
        }
#elif PLATFORM_WINDOWS
        const uint32 FileAttributes = GetFileAttributesW(*CurrentPath);
        if (FileAttributes != 0xFFFFFFFF
            && (FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        {
            return true;
        }
#elif ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
        if (PlatformFile.IsSymlink(*CurrentPath)
            != ESymlinkResult::NotSymlink)
        {
            return true;
        }
#else
        return true;
#endif
        if (!PlatformFile.FileExists(*CurrentPath)
            && !PlatformFile.DirectoryExists(*CurrentPath))
        {
            break;
        }
    }
    return false;
}
}

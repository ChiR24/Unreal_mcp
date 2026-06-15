#include "Acp/StudioKit/UnrealAgentStudioKitPrivate.h"

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

namespace UnrealAgentStudioKit
{
bool IsTemplatePathFreeOfLinks(
    const FString& ProjectDirectory,
    const FString& Path)
{
    FString NormalizedProject = ProjectDirectory;
    FString NormalizedPath = Path;
    FPaths::NormalizeDirectoryName(NormalizedProject);
    FPaths::NormalizeFilename(NormalizedPath);
    if (!NormalizedProject.EndsWith(TEXT("/")))
    {
        NormalizedProject += TEXT("/");
    }
    if (!NormalizedPath.StartsWith(NormalizedProject, ESearchCase::IgnoreCase))
    {
        return false;
    }

    TArray<FString> Segments;
    NormalizedPath.RightChop(NormalizedProject.Len()).ParseIntoArray(
        Segments,
        TEXT("/"),
        true);
    FString CurrentPath = NormalizedProject.LeftChop(1);
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
                return false;
            }
        }
        else if (errno != ENOENT)
        {
            return false;
        }
#elif PLATFORM_WINDOWS
        const uint32 FileAttributes = GetFileAttributesW(*CurrentPath);
        if (FileAttributes != 0xFFFFFFFF
            && (FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        {
            return false;
        }
#elif ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
        if (PlatformFile.IsSymlink(*CurrentPath) != ESymlinkResult::NotSymlink)
        {
            return false;
        }
#else
        return false;
#endif

        if (!PlatformFile.FileExists(*CurrentPath)
            && !PlatformFile.DirectoryExists(*CurrentPath))
        {
            break;
        }
    }
    return true;
}
}

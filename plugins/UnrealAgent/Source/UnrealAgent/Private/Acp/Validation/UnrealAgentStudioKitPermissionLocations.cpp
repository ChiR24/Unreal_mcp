#include "Acp/Validation/UnrealAgentStudioKitPermissionLocations.h"

#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"

namespace UnrealAgent::Validation
{
namespace
{
void AddUniqueDirectory(
    TArray<FString>& Directories,
    const FString& ProjectDirectory,
    const FString& Directory)
{
    if (!Directory.IsEmpty())
    {
        Directories.AddUnique(ResolveOpenCodePath(ProjectDirectory, Directory));
    }
}
}

FString ResolveOpenCodePath(
    const FString& ProjectDirectory,
    const FString& Path)
{
    const FString AbsolutePath = FPaths::IsRelative(Path)
        ? FPaths::Combine(ProjectDirectory, Path)
        : Path;
    return FPaths::ConvertRelativePathToFull(AbsolutePath);
}

TArray<FString> GetOpenCodeConfigDirectories(const FString& ProjectDirectory)
{
    TArray<FString> Directories;
    AddUniqueDirectory(
        Directories,
        ProjectDirectory,
        FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_CONFIG_DIR")));
    const FString XdgConfigHome =
        FPlatformMisc::GetEnvironmentVariable(TEXT("XDG_CONFIG_HOME"));
    if (!XdgConfigHome.IsEmpty())
    {
        AddUniqueDirectory(
            Directories,
            ProjectDirectory,
            FPaths::Combine(XdgConfigHome, TEXT("opencode")));
    }
    const FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
    if (!Home.IsEmpty())
    {
        AddUniqueDirectory(
            Directories,
            ProjectDirectory,
            FPaths::Combine(Home, TEXT(".config/opencode")));
    }
    const FString AppData =
        FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"));
    if (!AppData.IsEmpty())
    {
        AddUniqueDirectory(
            Directories,
            ProjectDirectory,
            FPaths::Combine(AppData, TEXT("opencode")));
    }
    const FString LocalAppData =
        FPlatformMisc::GetEnvironmentVariable(TEXT("LOCALAPPDATA"));
    if (!LocalAppData.IsEmpty())
    {
        AddUniqueDirectory(
            Directories,
            ProjectDirectory,
            FPaths::Combine(LocalAppData, TEXT("opencode")));
    }
    return Directories;
}

TArray<FString> GetManagedOpenCodeConfigDirectories()
{
    TArray<FString> Directories;
#if PLATFORM_WINDOWS
    const FString ProgramData =
        FPlatformMisc::GetEnvironmentVariable(TEXT("PROGRAMDATA"));
    const FString ProgramDataRoot =
        ProgramData.IsEmpty() ? TEXT("C:/ProgramData") : ProgramData;
    AddUniqueDirectory(
        Directories,
        FString(),
        FPaths::Combine(ProgramDataRoot, TEXT("opencode")));
#elif PLATFORM_MAC
    AddUniqueDirectory(
        Directories,
        FString(),
        TEXT("/Library/Application Support/opencode"));
#else
    AddUniqueDirectory(Directories, FString(), TEXT("/etc/opencode"));
#endif
    return Directories;
}
}

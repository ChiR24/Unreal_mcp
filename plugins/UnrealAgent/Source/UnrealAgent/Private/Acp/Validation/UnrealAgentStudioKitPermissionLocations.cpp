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
    // Only the explicit OPENCODE_CONFIG_DIR env var is treated as a scanned source.
    // XDG, HOME/.config/opencode, APPDATA, and LOCALAPPDATA are deferred to the
    // developer's own OpenCode setup: the Unreal Agent panel does not enforce the
    // permission safety policy on user-level OpenCode config files because users
    // intentionally install MCP servers and plugins there. Project-level safety is
    // preserved via the project-local, ancestor, managed, and OPENCODE_CONFIG_DIR
    // checks; the explicit OPENCODE_CONFIG_DIR is honored because it is the user's
    // intentional override of the project context.
    AddUniqueDirectory(
        Directories,
        ProjectDirectory,
        FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_CONFIG_DIR")));
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

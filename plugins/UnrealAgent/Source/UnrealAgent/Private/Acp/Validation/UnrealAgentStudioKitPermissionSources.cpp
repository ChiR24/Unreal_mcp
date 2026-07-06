#include "Acp/Validation/UnrealAgentStudioKitValidationChecks.h"

#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Acp/Validation/UnrealAgentStudioKitPermissionLocations.h"
#include "Acp/Validation/UnrealAgentStudioKitPermissionPluginSources.h"
#include "Acp/Validation/UnrealAgentValidationRunner.h"

#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"

namespace UnrealAgent::Validation
{
namespace
{
void AddOptionalConfigFiles(
    FUnrealAgentValidationResult& Result,
    const FString& Directory)
{
    AddOptionalOpenCodePermissionCheck(
        Result,
        FPaths::Combine(Directory, TEXT("config.json")));
    AddOptionalOpenCodePermissionCheck(
        Result,
        FPaths::Combine(Directory, TEXT("opencode.json")));
    AddOptionalOpenCodePermissionCheck(
        Result,
        FPaths::Combine(Directory, TEXT("opencode.jsonc")));
}

void AddOptionalAgentDirectoryChecks(
    FUnrealAgentValidationResult& Result,
    const FString& Directory)
{
    if (FPaths::DirectoryExists(Directory))
    {
        AddOpenCodeAgentPermissionChecks(Result, Directory, false);
    }
}

bool IsGitBoundary(const FString& Directory)
{
    const FString GitPath = FPaths::Combine(Directory, TEXT(".git"));
    return FPaths::DirectoryExists(GitPath) || FPaths::FileExists(GitPath);
}

void AddAncestorProjectChecks(
    FUnrealAgentValidationResult& Result,
    const FString& ProjectDirectory)
{
    FString Current = FPaths::ConvertRelativePathToFull(ProjectDirectory);
    while (!Current.IsEmpty() && !IsGitBoundary(Current))
    {
        const FString Parent = FPaths::GetPath(Current);
        if (Parent.IsEmpty() || Parent == Current)
        {
            break;
        }
        Current = Parent;
        AddOptionalConfigFiles(Result, Current);
        AddOptionalOpenCodePermissionCheck(
            Result,
            FPaths::Combine(Current, TEXT(".opencode/opencode.json")));
        AddOptionalOpenCodePermissionCheck(
            Result,
            FPaths::Combine(Current, TEXT(".opencode/opencode.jsonc")));
        AddOptionalAgentDirectoryChecks(
            Result,
            FPaths::Combine(Current, TEXT(".opencode/agents")));
        AddOptionalAgentDirectoryChecks(
            Result,
            FPaths::Combine(Current, TEXT(".opencode/agent")));
        const FString AncestorPluginsDirectory =
            FPaths::Combine(Current, TEXT(".opencode/plugins"));
        const FString AncestorGuardrailsPath = FPaths::Combine(
            AncestorPluginsDirectory,
            TEXT("unreal-agent-guardrails.ts"));
        if (FPaths::FileExists(AncestorGuardrailsPath))
        {
            AddOpenCodePluginDirectoryChecks(
                Result,
                AncestorPluginsDirectory,
                AncestorGuardrailsPath,
                FUnrealAgentStudioKit::MakeGuardrailsPluginSource());
        }
        else
        {
            AddOpenCodePluginDirectoryChecks(Result, AncestorPluginsDirectory);
        }
        AddOpenCodePluginDirectoryChecks(
            Result,
            FPaths::Combine(Current, TEXT(".opencode/plugin")));
        if (IsGitBoundary(Current))
        {
            break;
        }
    }
}

bool IsEnabledEnvironmentFlag(FString Value)
{
    Value = Value.TrimStartAndEnd().ToLower();
    return !Value.IsEmpty()
        && Value != TEXT("0")
        && Value != TEXT("false")
        && Value != TEXT("no")
        && Value != TEXT("off");
}

void AddEnvironmentOverrideChecks(FUnrealAgentValidationResult& Result)
{
    const FString Permission =
        FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_PERMISSION"));
    if (!Permission.IsEmpty())
    {
        AddOptionalOpenCodePermissionTextCheck(
            Result,
            FString::Printf(TEXT("{\"permission\":%s}"), *Permission),
            TEXT("OPENCODE_PERMISSION"));
    }
    if (IsEnabledEnvironmentFlag(
            FPlatformMisc::GetEnvironmentVariable(
                TEXT("OPENCODE_DISABLE_PROJECT_CONFIG"))))
    {
        Result.bPassed = false;
        Result.Errors.Add(
            TEXT("OPENCODE_DISABLE_PROJECT_CONFIG disables required Unreal Agent guardrails."));
    }
    if (IsEnabledEnvironmentFlag(
            FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_PURE"))))
    {
        Result.bPassed = false;
        Result.Errors.Add(
            TEXT("OPENCODE_PURE disables required Unreal Agent plugins and guardrails."));
    }
}
}

bool ValidateOpenCodePermissionSafety(
    const FString& ProjectDirectory,
    TArray<FString>& OutErrors,
    const TArray<FString>* ManagedConfigDirectoriesOverride)
{
    FUnrealAgentValidationResult Result;
    const FString TrustedPluginPath = FPaths::Combine(
        ProjectDirectory,
        TEXT(".opencode/plugins/unreal-agent-guardrails.ts"));
    AddOpenCodePluginDirectoryChecks(
        Result,
        FPaths::GetPath(TrustedPluginPath),
        TrustedPluginPath,
        FUnrealAgentStudioKit::MakeGuardrailsPluginSource());
    AddOpenCodePluginDirectoryChecks(
        Result,
        FPaths::Combine(ProjectDirectory, TEXT(".opencode/plugin")));
    AddOpenCodePermissionCheck(
        Result,
        FPaths::Combine(ProjectDirectory, TEXT(".opencode/opencode.json")));
    AddOpenCodeAgentPermissionChecks(
        Result,
        FPaths::Combine(ProjectDirectory, TEXT(".opencode/agents")));
    AddOptionalAgentDirectoryChecks(
        Result,
        FPaths::Combine(ProjectDirectory, TEXT(".opencode/agent")));

    AddOptionalConfigFiles(Result, ProjectDirectory);
    AddOptionalOpenCodePermissionCheck(
        Result,
        FPaths::Combine(ProjectDirectory, TEXT(".opencode/opencode.jsonc")));
    AddAncestorProjectChecks(Result, ProjectDirectory);
    AddEnvironmentOverrideChecks(Result);

    const TArray<FString> ManagedConfigDirectories =
        ManagedConfigDirectoriesOverride != nullptr
        ? *ManagedConfigDirectoriesOverride
        : GetManagedOpenCodeConfigDirectories();
    for (const FString& ConfigDirectory : ManagedConfigDirectories)
    {
        AddOptionalConfigFiles(Result, ConfigDirectory);
        AddOptionalAgentDirectoryChecks(
            Result,
            FPaths::Combine(ConfigDirectory, TEXT("agents")));
        AddOptionalAgentDirectoryChecks(
            Result,
            FPaths::Combine(ConfigDirectory, TEXT("agent")));
        AddOpenCodePluginDirectoryChecks(
            Result,
            FPaths::Combine(ConfigDirectory, TEXT("plugins")));
        AddOpenCodePluginDirectoryChecks(
            Result,
            FPaths::Combine(ConfigDirectory, TEXT("plugin")));
    }

    const FString ExplicitConfig =
        FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_CONFIG"));
    if (!ExplicitConfig.IsEmpty())
    {
        const FString ExplicitConfigPath =
            ResolveOpenCodePath(ProjectDirectory, ExplicitConfig);
        if (FPaths::FileExists(ExplicitConfigPath))
        {
            AddOptionalOpenCodePermissionCheck(Result, ExplicitConfigPath);
        }
        else
        {
            Result.bPassed = false;
            Result.Errors.Add(FString::Printf(
                TEXT("OPENCODE_CONFIG file is unreadable: %s"),
                *ExplicitConfigPath));
        }
    }
    AddOptionalOpenCodePermissionTextCheck(
        Result,
        FPlatformMisc::GetEnvironmentVariable(TEXT("OPENCODE_CONFIG_CONTENT")),
        TEXT("OPENCODE_CONFIG_CONTENT"));

    for (const FString& ConfigDirectory : GetOpenCodeConfigDirectories(ProjectDirectory))
    {
        AddOptionalConfigFiles(Result, ConfigDirectory);
        AddOptionalAgentDirectoryChecks(
            Result,
            FPaths::Combine(ConfigDirectory, TEXT("agents")));
        AddOptionalAgentDirectoryChecks(
            Result,
            FPaths::Combine(ConfigDirectory, TEXT("agent")));
        AddOpenCodePluginDirectoryChecks(
            Result,
            FPaths::Combine(ConfigDirectory, TEXT("plugins")));
        AddOpenCodePluginDirectoryChecks(
            Result,
            FPaths::Combine(ConfigDirectory, TEXT("plugin")));
    }

    OutErrors = MoveTemp(Result.Errors);
    return Result.bPassed;
}
}

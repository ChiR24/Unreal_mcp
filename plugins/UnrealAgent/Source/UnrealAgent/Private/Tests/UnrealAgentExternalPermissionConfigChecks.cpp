#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentPermissionSafetyChecks.h"

#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Acp/Validation/UnrealAgentStudioKitValidationChecks.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunExternalPermissionConfigChecks(
        FAutomationTestBase& Test,
        const FString& RootDirectory,
        const FString& GlobalConfigDirectory,
        const FString& TestHomeDirectory)
    {
        bool bPassed = true;
        const FString GlobalConfigProject = FPaths::Combine(RootDirectory, TEXT("GlobalConfigProject"));
        FUnrealAgentStudioKit::EnsureForProject(GlobalConfigProject);
        const FString GlobalConfigPath = FPaths::Combine(GlobalConfigDirectory, TEXT("config.json"));
        bPassed &= Test.TestTrue(
            TEXT("Unsafe global config.json is seeded"),
            SavePermissionTestText(
                GlobalConfigPath,
                TEXT("{\"permission\":{\"unreal-engine*\":\"allow\"}}\n")));
        TArray<FString> GlobalConfigErrors;
        bPassed &= Test.TestFalse(
            TEXT("Unsafe global config.json permission is rejected"),
            UnrealAgent::Validation::ValidateOpenCodePermissionSafety(
                GlobalConfigProject,
                GlobalConfigErrors));
        IFileManager::Get().Delete(*GlobalConfigPath, false, true);

        const FString ManagedConfigProject = FPaths::Combine(RootDirectory, TEXT("ManagedConfigProject"));
        FUnrealAgentStudioKit::EnsureForProject(ManagedConfigProject);
        const FString ManagedConfigDirectory = FPaths::Combine(RootDirectory, TEXT("ManagedConfig"));
        bPassed &= Test.TestTrue(
            TEXT("Unsafe managed config is seeded"),
            SavePermissionTestText(
                FPaths::Combine(ManagedConfigDirectory, TEXT("opencode.json")),
                TEXT("{\"permission\":{\"unreal-engine*\":\"allow\"}}\n")));
        const TArray<FString> ManagedDirectories = {ManagedConfigDirectory};
        TArray<FString> ManagedConfigErrors;
        bPassed &= Test.TestFalse(
            TEXT("Unsafe managed OpenCode permission is rejected"),
            UnrealAgent::Validation::ValidateOpenCodePermissionSafety(
                ManagedConfigProject,
                ManagedConfigErrors,
                &ManagedDirectories));

        const FString HomeConfigProject = FPaths::Combine(RootDirectory, TEXT("HomeConfigProject"));
        FUnrealAgentStudioKit::EnsureForProject(HomeConfigProject);
        const FString HomeConfigPath = FPaths::Combine(TestHomeDirectory, TEXT(".opencode/opencode.jsonc"));
        bPassed &= Test.TestTrue(
            TEXT("Unsafe home .opencode JSONC config is seeded"),
            SavePermissionTestText(
                HomeConfigPath,
                TEXT("{\"permission\":{\"unreal-engine*\":\"allow\"}}\n")));
        TArray<FString> HomeConfigErrors;
        bPassed &= Test.TestFalse(
            TEXT("Unsafe home .opencode JSONC permission is rejected"),
            UnrealAgent::Validation::ValidateOpenCodePermissionSafety(
                HomeConfigProject,
                HomeConfigErrors));
        IFileManager::Get().Delete(*HomeConfigPath, false, true);

        const FString ExplicitConfigDirectory = FPaths::Combine(RootDirectory, TEXT("ExplicitConfig"));
        FUnrealAgentStudioKit::EnsureForProject(ExplicitConfigDirectory);
        const FString ExplicitConfigPath = FPaths::Combine(RootDirectory, TEXT("unsafe-explicit.json"));
        bPassed &= Test.TestTrue(
            TEXT("Unsafe explicit config is seeded"),
            SavePermissionTestText(
                ExplicitConfigPath,
                TEXT("{\"permission\":{\"execute_command\":\"allow\"}}\n")));
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_CONFIG"), *ExplicitConfigPath);
        TArray<FString> ExplicitConfigErrors;
        bPassed &= Test.TestFalse(
            TEXT("Unsafe OPENCODE_CONFIG permission is rejected"),
            UnrealAgent::Validation::ValidateOpenCodePermissionSafety(
                ExplicitConfigDirectory,
                ExplicitConfigErrors));
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_CONFIG"), TEXT(""));

        const FString GlobalAgentPath = FPaths::Combine(GlobalConfigDirectory, TEXT("agents/unsafe-global.md"));
        bPassed &= Test.TestTrue(
            TEXT("Unsafe global agent is seeded"),
            SavePermissionTestText(
                GlobalAgentPath,
                TEXT("---\ndescription: unsafe\npermission:\n  \"unreal-engine*\": allow\n---\n")));
        const FString GlobalAgentProject = FPaths::Combine(RootDirectory, TEXT("GlobalAgent"));
        FUnrealAgentStudioKit::EnsureForProject(GlobalAgentProject);
        TArray<FString> GlobalAgentErrors;
        bPassed &= Test.TestFalse(
            TEXT("Unsafe global OpenCode agent permission is rejected"),
            UnrealAgent::Validation::ValidateOpenCodePermissionSafety(
                GlobalAgentProject,
                GlobalAgentErrors));

        IFileManager::Get().Delete(*GlobalAgentPath, false, true);
        const FString GlobalPluginPath = FPaths::Combine(GlobalConfigDirectory, TEXT("plugins/untrusted.js"));
        bPassed &= Test.TestTrue(
            TEXT("Unsafe global plugin is seeded"),
            SavePermissionTestText(GlobalPluginPath, TEXT("export default async () => ({})\n")));
        TArray<FString> GlobalPluginErrors;
        bPassed &= Test.TestFalse(
            TEXT("Global OpenCode plugin is rejected"),
            UnrealAgent::Validation::ValidateOpenCodePermissionSafety(
                GlobalAgentProject,
                GlobalPluginErrors));
        return bPassed;
    }
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentPermissionSafetyChecks.h"

#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Acp/Validation/UnrealAgentStudioKitValidationChecks.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunProjectPermissionConfigChecks(
        FAutomationTestBase& Test,
        const FString& RootDirectory)
    {
        bool bPassed = true;
        const FString RootConfigDirectory = FPaths::Combine(RootDirectory, TEXT("RootConfig"));
        FUnrealAgentStudioKit::EnsureForProject(RootConfigDirectory);
        bPassed &= Test.TestTrue(
            TEXT("Unsafe root JSONC config is seeded"),
            SavePermissionTestText(
                FPaths::Combine(RootConfigDirectory, TEXT("opencode.jsonc")),
                TEXT("{\n// source override\n\"permission\":{\"unreal-engine*\":\"allow\",},\n}\n")));
        TArray<FString> RootConfigErrors;
        bPassed &= Test.TestFalse(
            TEXT("Unsafe root JSONC permission config is rejected"),
            UnrealAgent::Validation::ValidateOpenCodePermissionSafety(
                RootConfigDirectory,
                RootConfigErrors));

        const FString ProjectDotConfigDirectory = FPaths::Combine(RootDirectory, TEXT("ProjectDotConfig"));
        FUnrealAgentStudioKit::EnsureForProject(ProjectDotConfigDirectory);
        bPassed &= Test.TestTrue(
            TEXT("Unsafe project .opencode JSONC config is seeded"),
            SavePermissionTestText(
                FPaths::Combine(ProjectDotConfigDirectory, TEXT(".opencode/opencode.jsonc")),
                TEXT("{\"permission\":{\"unreal-engine*\":\"allow\"}}\n")));
        TArray<FString> ProjectDotConfigErrors;
        bPassed &= Test.TestFalse(
            TEXT("Unsafe project .opencode JSONC permission config is rejected"),
            UnrealAgent::Validation::ValidateOpenCodePermissionSafety(
                ProjectDotConfigDirectory,
                ProjectDotConfigErrors));

        const FString LegacyToolsDirectory = FPaths::Combine(RootDirectory, TEXT("LegacyTools"));
        FUnrealAgentStudioKit::EnsureForProject(LegacyToolsDirectory);
        bPassed &= Test.TestTrue(
            TEXT("Unsafe legacy tools config is seeded"),
            SavePermissionTestText(
                FPaths::Combine(LegacyToolsDirectory, TEXT("opencode.json")),
                TEXT("{\"tools\":{\"dangerous_local_tool\":true}}\n")));
        TArray<FString> LegacyToolsErrors;
        bPassed &= Test.TestFalse(
            TEXT("Unsafe legacy OpenCode tools policy is rejected"),
            UnrealAgent::Validation::ValidateOpenCodePermissionSafety(
                LegacyToolsDirectory,
                LegacyToolsErrors));

        const FString ConfiguredPluginDirectory = FPaths::Combine(RootDirectory, TEXT("ConfiguredPlugin"));
        FUnrealAgentStudioKit::EnsureForProject(ConfiguredPluginDirectory);
        bPassed &= Test.TestTrue(
            TEXT("Configured plugin override is seeded"),
            SavePermissionTestText(
                FPaths::Combine(ConfiguredPluginDirectory, TEXT("opencode.json")),
                TEXT("{\"plugin\":[\"file:///tmp/untrusted-plugin.js\"]}\n")));
        TArray<FString> ConfiguredPluginErrors;
        bPassed &= Test.TestFalse(
            TEXT("Configured OpenCode plugin is rejected"),
            UnrealAgent::Validation::ValidateOpenCodePermissionSafety(
                ConfiguredPluginDirectory,
                ConfiguredPluginErrors));

        const FString ConfiguredMcpDirectory = FPaths::Combine(RootDirectory, TEXT("ConfiguredMcp"));
        FUnrealAgentStudioKit::EnsureForProject(ConfiguredMcpDirectory);
        bPassed &= Test.TestTrue(
            TEXT("Configured local MCP server is seeded"),
            SavePermissionTestText(
                FPaths::Combine(ConfiguredMcpDirectory, TEXT("opencode.json")),
                TEXT("{\"mcp\":{\"payload\":{\"type\":\"local\",\"command\":[\"/tmp/payload\"]}}}\n")));
        TArray<FString> ConfiguredMcpErrors;
        bPassed &= Test.TestFalse(
            TEXT("Configured local OpenCode MCP server is rejected"),
            UnrealAgent::Validation::ValidateOpenCodePermissionSafety(
                ConfiguredMcpDirectory,
                ConfiguredMcpErrors));

        const FString ExtraPluginDirectory = FPaths::Combine(RootDirectory, TEXT("ExtraPlugin"));
        FUnrealAgentStudioKit::EnsureForProject(ExtraPluginDirectory);
        bPassed &= Test.TestTrue(
            TEXT("Extra project plugin is seeded"),
            SavePermissionTestText(
                FPaths::Combine(ExtraPluginDirectory, TEXT(".opencode/plugins/extra.ts")),
                TEXT("export const Extra = async () => ({})\n")));
        TArray<FString> ExtraPluginErrors;
        bPassed &= Test.TestFalse(
            TEXT("Extra project OpenCode plugin is rejected"),
            UnrealAgent::Validation::ValidateOpenCodePermissionSafety(
                ExtraPluginDirectory,
                ExtraPluginErrors));
        return bPassed;
    }
}

#endif

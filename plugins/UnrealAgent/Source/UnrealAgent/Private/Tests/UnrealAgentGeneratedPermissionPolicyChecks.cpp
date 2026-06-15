#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentPermissionSafetyChecks.h"

#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Acp/Validation/UnrealAgentStudioKitValidationChecks.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunGeneratedPermissionPolicyChecks(
        FAutomationTestBase& Test,
        const FString& RootDirectory)
    {
        bool bPassed = true;
        const FString GeneratedPolicyDirectory = FPaths::Combine(RootDirectory, TEXT("GeneratedPolicy"));
        FUnrealAgentStudioKit::EnsureForProject(GeneratedPolicyDirectory);
        TArray<FString> GeneratedPolicyErrors;
        bPassed &= Test.TestTrue(
            TEXT("Generated Studio Kit permission policy is accepted"),
            UnrealAgent::Validation::ValidateOpenCodePermissionSafety(
                GeneratedPolicyDirectory,
                GeneratedPolicyErrors));
        bPassed &= Test.TestTrue(
            TEXT("Quoted agent allow is rejected"),
            ValidatePermissionAgentVariant(
                Test,
                RootDirectory,
                TEXT("QuotedAllow"),
                TEXT("---\ndescription: unsafe\npermission:\n  unreal-engine_manage_asset: \"allow\"\n---\n")));
        bPassed &= Test.TestTrue(
            TEXT("Inline agent permission map is rejected"),
            ValidatePermissionAgentVariant(
                Test,
                RootDirectory,
                TEXT("InlineAllow"),
                TEXT("---\ndescription: unsafe\npermission: { unreal-engine_manage_asset: allow }\n---\n")));
        bPassed &= Test.TestTrue(
            TEXT("Wildcard agent permission overlap is rejected"),
            ValidatePermissionAgentVariant(
                Test,
                RootDirectory,
                TEXT("WildcardAllow"),
                TEXT("---\ndescription: unsafe\npermission:\n  \"unreal-engine*\": allow\n---\n")));
        bPassed &= Test.TestTrue(
            TEXT("Unknown Unreal MCP parent permission is rejected"),
            ValidatePermissionAgentVariant(
                Test,
                RootDirectory,
                TEXT("UnknownMcpParent"),
                TEXT("---\ndescription: unsafe\npermission:\n  unreal-engine_custom_mutator: allow\n---\n")));
        bPassed &= Test.TestTrue(
            TEXT("Unknown local tool permission is rejected"),
            ValidatePermissionAgentVariant(
                Test,
                RootDirectory,
                TEXT("UnknownLocalTool"),
                TEXT("---\ndescription: unsafe\npermission:\n  dangerous_local_tool: allow\n---\n")));
        bPassed &= Test.TestTrue(
            TEXT("Missing agent permission policy is rejected"),
            ValidatePermissionAgentVariant(
                Test,
                RootDirectory,
                TEXT("MissingPolicy"),
                TEXT("---\ndescription: unsafe\nmode: primary\n---\n")));
        bPassed &= Test.TestTrue(
            TEXT("Folded agent permission scalar is rejected"),
            ValidatePermissionAgentVariant(
                Test,
                RootDirectory,
                TEXT("FoldedAllow"),
                TEXT("---\ndescription: unsafe\npermission:\n  unreal-engine_manage_asset: >-\n    allow\n---\n")));
        bPassed &= Test.TestTrue(
            TEXT("Escaped agent permission scalar is rejected"),
            ValidatePermissionAgentVariant(
                Test,
                RootDirectory,
                TEXT("EscapedAllow"),
                TEXT("---\ndescription: unsafe\npermission:\n  unreal-engine_manage_asset: \"a\\x6clow\"\n---\n")));
        bPassed &= Test.TestTrue(
            TEXT("Nested permission decoy does not hide unsafe root policy"),
            ValidatePermissionAgentVariant(
                Test,
                RootDirectory,
                TEXT("NestedDecoy"),
                TEXT("---\ndescription: unsafe\nmetadata:\n  permission:\n    \"*\": ask\npermission:\n  unreal-engine_manage_asset: allow\n---\n")));

        const FString InlineConfigDirectory = FPaths::Combine(RootDirectory, TEXT("InlineConfig"));
        FUnrealAgentStudioKit::EnsureForProject(InlineConfigDirectory);
        FPlatformMisc::SetEnvironmentVar(
            TEXT("OPENCODE_CONFIG_CONTENT"),
            TEXT("{\"permission\":{\"unreal-engine*\":\"allow\"}}"));
        TArray<FString> InlineErrors;
        bPassed &= Test.TestFalse(
            TEXT("Unsafe inline OpenCode permission config is rejected"),
            UnrealAgent::Validation::ValidateOpenCodePermissionSafety(
                InlineConfigDirectory,
                InlineErrors));
        FPlatformMisc::SetEnvironmentVar(TEXT("OPENCODE_CONFIG_CONTENT"), TEXT(""));
        return bPassed;
    }
}

#endif

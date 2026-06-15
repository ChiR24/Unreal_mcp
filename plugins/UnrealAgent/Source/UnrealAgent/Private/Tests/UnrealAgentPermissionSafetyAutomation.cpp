#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentAcpProtocolTestHelpers.h"
#include "Tests/UnrealAgentPermissionSafetyChecks.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnrealAgentPermissionSafetyTest,
    "UnrealAgent.Acp.PermissionSafety",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUnrealAgentPermissionSafetyTest::RunTest(const FString& Parameters)
{
    const FString RootDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentPermissionSafetyHarness")));
    IFileManager::Get().DeleteDirectory(*RootDirectory, false, true);
    IFileManager::Get().MakeDirectory(*RootDirectory, true);

    UnrealAgent::AutomationTests::FScopedOpenCodeConfigEnvironment ConfigEnvironment(RootDirectory);
    const FString GlobalConfigDirectory = FPaths::Combine(RootDirectory, TEXT("GlobalConfig"));
    const FString TestHomeDirectory = FPaths::Combine(RootDirectory, TEXT("Home"));

    bool bPassed = true;
    bPassed &= UnrealAgent::AutomationTests::RunGeneratedPermissionPolicyChecks(*this, RootDirectory);
    bPassed &= UnrealAgent::AutomationTests::RunProjectPermissionConfigChecks(*this, RootDirectory);
    bPassed &= UnrealAgent::AutomationTests::RunExternalPermissionConfigChecks(
        *this,
        RootDirectory,
        GlobalConfigDirectory,
        TestHomeDirectory);

    IFileManager::Get().DeleteDirectory(*RootDirectory, false, true);
    return bPassed;
}

#endif

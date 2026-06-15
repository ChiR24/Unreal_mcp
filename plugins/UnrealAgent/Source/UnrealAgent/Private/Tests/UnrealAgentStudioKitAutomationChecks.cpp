#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentAutomationTestDelegates.h"
#include "Tests/UnrealAgentAcpProtocolTestHelpers.h"
#include "Tests/UnrealAgentStudioKitTestChecks.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunStudioKitAndContextTest(FAutomationTestBase& Test)
    {
        const FString TestDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitTest")));
        IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
        if (!IFileManager::Get().MakeDirectory(*TestDirectory, true))
        {
            Test.AddError(FString::Printf(
                TEXT("Failed to create Studio Kit test directory: %s"),
                *TestDirectory));
            return false;
        }
        FScopedOpenCodeConfigEnvironment ConfigEnvironment(TestDirectory);

        bool bPassed = true;
        bPassed &= RunStudioKitGeneratedArtifactChecks(Test, TestDirectory);
        bPassed &= RunStudioKitCoreSkillContentChecks(Test, TestDirectory);
        bPassed &= RunStudioKitDomainSkillContentChecks(Test, TestDirectory);
        bPassed &= RunStudioKitWorkflowCommandChecks(Test, TestDirectory);
        bPassed &= RunStudioKitGuardrailContentChecks(Test, TestDirectory);
        bPassed &= RunStudioKitRedactionContextChecks(Test, TestDirectory);
        bPassed &= RunStudioKitEvidenceValidationChecks(Test, TestDirectory);
        bPassed &= RunStudioKitOwnershipChecks(Test);
        bPassed &= RunStudioKitAtomicReplacementChecks(Test);
        bPassed &= RunStudioKitAtomicRollbackChecks(Test);
        bPassed &= RunStudioKitPermissionValidationChecks(Test);
        bPassed &= RunStudioKitGeneratedPathSafetyChecks(Test);

        IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
        return bPassed;
    }
}

#endif

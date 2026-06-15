#if WITH_DEV_AUTOMATION_TESTS

#pragma once

#include "CoreMinimal.h"

class FAutomationTestBase;

namespace UnrealAgent::AutomationTests
{
    bool RunStudioKitGeneratedArtifactChecks(FAutomationTestBase& Test, const FString& TestDirectory);
    bool RunStudioKitCoreSkillContentChecks(FAutomationTestBase& Test, const FString& TestDirectory);
    bool RunStudioKitDomainSkillContentChecks(FAutomationTestBase& Test, const FString& TestDirectory);
    bool RunStudioKitWorkflowCommandChecks(FAutomationTestBase& Test, const FString& TestDirectory);
    bool RunStudioKitGuardrailContentChecks(FAutomationTestBase& Test, const FString& TestDirectory);
    bool RunStudioKitRedactionContextChecks(FAutomationTestBase& Test, const FString& TestDirectory);
    bool RunStudioKitEvidenceValidationChecks(FAutomationTestBase& Test, const FString& TestDirectory);
    bool RunStudioKitOwnershipChecks(FAutomationTestBase& Test);
    bool RunStudioKitAtomicReplacementChecks(FAutomationTestBase& Test);
    bool RunStudioKitAtomicRollbackChecks(FAutomationTestBase& Test);
    bool RunStudioKitPermissionValidationChecks(FAutomationTestBase& Test);
    bool RunStudioKitGeneratedPathSafetyChecks(FAutomationTestBase& Test);
}

#endif

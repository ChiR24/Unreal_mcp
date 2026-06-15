#if WITH_DEV_AUTOMATION_TESTS

#pragma once

#include "CoreMinimal.h"

class FAutomationTestBase;

namespace UnrealAgent::AutomationTests
{
    bool ValidatePermissionAgentVariant(
        FAutomationTestBase& Test,
        const FString& RootDirectory,
        const FString& Name,
        const FString& AgentText);
    bool SavePermissionTestText(const FString& Path, const FString& Text);

    bool RunGeneratedPermissionPolicyChecks(
        FAutomationTestBase& Test,
        const FString& RootDirectory);
    bool RunProjectPermissionConfigChecks(
        FAutomationTestBase& Test,
        const FString& RootDirectory);
    bool RunExternalPermissionConfigChecks(
        FAutomationTestBase& Test,
        const FString& RootDirectory,
        const FString& GlobalConfigDirectory,
        const FString& TestHomeDirectory);
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentPermissionSafetyChecks.h"

#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Acp/Validation/UnrealAgentStudioKitValidationChecks.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool ValidatePermissionAgentVariant(
        FAutomationTestBase& Test,
        const FString& RootDirectory,
        const FString& Name,
        const FString& AgentText)
    {
        const FString ProjectDirectory = FPaths::Combine(RootDirectory, Name);
        FUnrealAgentStudioKit::EnsureForProject(ProjectDirectory);
        const FString AgentPath =
            FPaths::Combine(ProjectDirectory, TEXT(".opencode/agents/unreal-agent.md"));
        if (!FFileHelper::SaveStringToFile(
                AgentText,
                *AgentPath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            Test.AddError(FString::Printf(TEXT("Failed to seed permission variant: %s"), *Name));
            return false;
        }

        TArray<FString> Errors;
        return !UnrealAgent::Validation::ValidateOpenCodePermissionSafety(ProjectDirectory, Errors);
    }

    bool SavePermissionTestText(const FString& Path, const FString& Text)
    {
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
        return FFileHelper::SaveStringToFile(
            Text,
            *Path,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}

#endif

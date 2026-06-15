#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentStudioKitTestChecks.h"

#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Acp/StudioKit/UnrealAgentStudioKitPrivate.h"
#include "Acp/Validation/UnrealAgentValidationRunner.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunStudioKitOwnershipChecks(FAutomationTestBase& Test)
    {
        bool bPassed = true;
        const FString CustomDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitCustomTest")));
        IFileManager::Get().DeleteDirectory(*CustomDirectory, false, true);
        const FString CustomAgentPath =
            FPaths::Combine(CustomDirectory, TEXT(".opencode/agents/unreal-agent.md"));
        const FString CustomConfigPath =
            FPaths::Combine(CustomDirectory, TEXT(".opencode/opencode.json"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(CustomAgentPath), true);
        const FString CustomAgentText = TEXT("custom user-owned Unreal Agent prompt");
        const FString CustomConfigText = TEXT("{\n  \"$schema\": \"https://opencode.ai/config.json\",\n  \"permission\": {\n    \"read\": \"deny\"\n  }\n}\n");
        FFileHelper::SaveStringToFile(CustomAgentText, *CustomAgentPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        FFileHelper::SaveStringToFile(CustomConfigText, *CustomConfigPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        FUnrealAgentStudioKit::EnsureForProject(CustomDirectory);
        FString PreservedAgentText;
        FString PreservedConfigText;
        bPassed &= Test.TestTrue(TEXT("Custom prompt remains readable"), FFileHelper::LoadFileToString(PreservedAgentText, *CustomAgentPath));
        bPassed &= Test.TestEqual(TEXT("Custom unmarked prompt is preserved"), PreservedAgentText, CustomAgentText);
        bPassed &= Test.TestTrue(TEXT("Custom OpenCode config remains readable"), FFileHelper::LoadFileToString(PreservedConfigText, *CustomConfigPath));
        bPassed &= Test.TestEqual(TEXT("Custom unmarked OpenCode config is preserved"), PreservedConfigText, CustomConfigText);

        const FString UnsafeConfigDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitUnsafeConfigTest")));
        IFileManager::Get().DeleteDirectory(*UnsafeConfigDirectory, false, true);
        FUnrealAgentStudioKit::EnsureForProject(UnsafeConfigDirectory);
        const FString UnsafeConfigPath =
            FPaths::Combine(UnsafeConfigDirectory, TEXT(".opencode/opencode.json"));
        const FString UnsafeConfigText = TEXT("{\n  \"$schema\": \"https://opencode.ai/config.json\",\n  \"permission\": {\n    \"*\": \"allow\"\n  }\n}\n");
        bPassed &= Test.TestTrue(TEXT("Unsafe custom OpenCode config is seeded"), FFileHelper::SaveStringToFile(UnsafeConfigText, *UnsafeConfigPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        FUnrealAgentStudioKit::EnsureForProject(UnsafeConfigDirectory);
        FString PreservedUnsafeConfigText;
        bPassed &= Test.TestTrue(TEXT("Unsafe custom OpenCode config remains readable"), FFileHelper::LoadFileToString(PreservedUnsafeConfigText, *UnsafeConfigPath));
        bPassed &= Test.TestEqual(TEXT("Unsafe unmarked OpenCode config is preserved for user ownership"), PreservedUnsafeConfigText, UnsafeConfigText);
        const FUnrealAgentValidationResult UnsafeConfigValidation =
            FUnrealAgentValidationRunner::RunFastValidation(UnsafeConfigDirectory);
        bPassed &= Test.TestFalse(TEXT("Unsafe preserved OpenCode config fails validation"), UnsafeConfigValidation.bPassed);
        bPassed &= Test.TestTrue(TEXT("Unsafe preserved OpenCode config reports explicit permission error"), UnsafeConfigValidation.Errors.ContainsByPredicate([](const FString& Error)
        {
            return Error.Contains(TEXT("Unsafe OpenCode config"));
        }));

        const FString OwnershipRaceDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitOwnershipRaceTest")));
        IFileManager::Get().DeleteDirectory(*OwnershipRaceDirectory, false, true);
        const FUnrealAgentStudioKitResult OwnershipSeedResult =
            FUnrealAgentStudioKit::EnsureForProject(OwnershipRaceDirectory);
        bPassed &= Test.TestTrue(TEXT("Ownership-race fixture starts from a managed Studio Kit"), OwnershipSeedResult.WasSuccessful());
        const FString OwnershipRacePath =
            FPaths::Combine(OwnershipRaceDirectory, TEXT(".opencode/agents/unreal-agent.md"));
        const FString OwnershipRaceOldManagedText =
            TEXT("unreal_agent_studio_kit_version: 1\nOld managed content.\n");
        const FString OwnershipRaceUserText =
            TEXT("---\ndescription: user-owned replacement\n---\nDo not overwrite this file.\n");
        bPassed &= Test.TestTrue(
            TEXT("Ownership-race target is made stale but still managed"),
            FFileHelper::SaveStringToFile(
                OwnershipRaceOldManagedText,
                *OwnershipRacePath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        bool bOwnershipRaceTriggered = false;
        UnrealAgentStudioKit::GBeforeStudioKitTemplateAtomicWriteForTest =
            [&bOwnershipRaceTriggered, OwnershipRacePath, OwnershipRaceUserText](const FString& Path)
            {
                if (bOwnershipRaceTriggered || Path != OwnershipRacePath)
                {
                    return;
                }
                bOwnershipRaceTriggered = FFileHelper::SaveStringToFile(
                    OwnershipRaceUserText,
                    *OwnershipRacePath,
                    FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
            };
        const FUnrealAgentStudioKitResult OwnershipRaceResult =
            FUnrealAgentStudioKit::EnsureForProject(OwnershipRaceDirectory);
        UnrealAgentStudioKit::GBeforeStudioKitTemplateAtomicWriteForTest = nullptr;
        FString OwnershipRaceFinalText;
        bPassed &= Test.TestTrue(TEXT("Ownership-race replacement fixture is triggered"), bOwnershipRaceTriggered);
        bPassed &= Test.TestTrue(TEXT("Ownership-race target remains readable"), FFileHelper::LoadFileToString(OwnershipRaceFinalText, *OwnershipRacePath));
        bPassed &= Test.TestEqual(TEXT("Studio Kit preserves an unmarked file installed after ownership classification"), OwnershipRaceFinalText, OwnershipRaceUserText);
        bPassed &= Test.TestTrue(TEXT("Ownership-race preservation remains a successful Studio Kit result"), OwnershipRaceResult.WasSuccessful());
        bPassed &= Test.TestTrue(TEXT("Ownership-race target is reported as preserved"), OwnershipRaceResult.PreservedPaths.Contains(OwnershipRacePath));
        IFileManager::Get().DeleteDirectory(*OwnershipRaceDirectory, false, true);

        const FString OwnershipDeletionDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitOwnershipDeletionTest")));
        IFileManager::Get().DeleteDirectory(*OwnershipDeletionDirectory, false, true);
        bPassed &= Test.TestTrue(
            TEXT("Ownership-deletion fixture starts from a managed Studio Kit"),
            FUnrealAgentStudioKit::EnsureForProject(OwnershipDeletionDirectory).WasSuccessful());
        const FString OwnershipDeletionPath =
            FPaths::Combine(OwnershipDeletionDirectory, TEXT(".opencode/agents/unreal-agent.md"));
        bPassed &= Test.TestTrue(
            TEXT("Ownership-deletion target is made stale but managed"),
            FFileHelper::SaveStringToFile(
                OwnershipRaceOldManagedText,
                *OwnershipDeletionPath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        bool bOwnershipDeletionTriggered = false;
        UnrealAgentStudioKit::GBeforeStudioKitTemplateAtomicWriteForTest =
            [&bOwnershipDeletionTriggered, OwnershipDeletionPath](const FString& Path)
            {
                if (bOwnershipDeletionTriggered || Path != OwnershipDeletionPath)
                {
                    return;
                }
                bOwnershipDeletionTriggered = IFileManager::Get().Delete(*OwnershipDeletionPath);
            };
        const FUnrealAgentStudioKitResult OwnershipDeletionResult =
            FUnrealAgentStudioKit::EnsureForProject(OwnershipDeletionDirectory);
        UnrealAgentStudioKit::GBeforeStudioKitTemplateAtomicWriteForTest = nullptr;
        bPassed &= Test.TestTrue(TEXT("Ownership-deletion fixture is triggered"), bOwnershipDeletionTriggered);
        bPassed &= Test.TestFalse(TEXT("Concurrent deletion is not reported as successful preservation"), OwnershipDeletionResult.WasSuccessful());
        bPassed &= Test.TestTrue(TEXT("Concurrent deletion is reported as failed"), OwnershipDeletionResult.FailedPaths.Contains(OwnershipDeletionPath));
        bPassed &= Test.TestFalse(TEXT("Concurrent deletion target remains absent"), FPaths::FileExists(OwnershipDeletionPath));
        IFileManager::Get().DeleteDirectory(*OwnershipDeletionDirectory, false, true);

        IFileManager::Get().DeleteDirectory(*CustomDirectory, false, true);
        IFileManager::Get().DeleteDirectory(*UnsafeConfigDirectory, false, true);
        return bPassed;
    }
}

#endif

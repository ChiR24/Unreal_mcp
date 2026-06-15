#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentStudioKitTestChecks.h"

#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Acp/Validation/UnrealAgentValidationRunner.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunStudioKitPermissionValidationChecks(FAutomationTestBase& Test)
    {
        bool bPassed = true;
        const FString MissingPermissionDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitMissingPermissionTest")));
        IFileManager::Get().DeleteDirectory(*MissingPermissionDirectory, false, true);
        FUnrealAgentStudioKit::EnsureForProject(MissingPermissionDirectory);
        const FString MissingPermissionConfigPath =
            FPaths::Combine(MissingPermissionDirectory, TEXT(".opencode/opencode.json"));
        bPassed &= Test.TestTrue(TEXT("Missing-permission config is seeded"), FFileHelper::SaveStringToFile(TEXT("{\n  \"$schema\": \"https://opencode.ai/config.json\"\n}\n"), *MissingPermissionConfigPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        const FUnrealAgentValidationResult MissingPermissionValidation =
            FUnrealAgentValidationRunner::RunFastValidation(MissingPermissionDirectory);
        bPassed &= Test.TestFalse(TEXT("OpenCode config without explicit permissions fails validation"), MissingPermissionValidation.bPassed);

        const FString NestedPermissionDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitNestedPermissionTest")));
        IFileManager::Get().DeleteDirectory(*NestedPermissionDirectory, false, true);
        FUnrealAgentStudioKit::EnsureForProject(NestedPermissionDirectory);
        const FString NestedPermissionConfigPath =
            FPaths::Combine(NestedPermissionDirectory, TEXT(".opencode/opencode.json"));
        const FString NestedPermissionConfig = TEXT("{\n  \"$schema\": \"https://opencode.ai/config.json\",\n  \"permission\": {\"*\": \"ask\"},\n  \"agent\": {\"unreal-agent\": {\"permission\": {\"execute_command\": \"allow\"}}}\n}\n");
        bPassed &= Test.TestTrue(TEXT("Nested unsafe permission config is seeded"), FFileHelper::SaveStringToFile(NestedPermissionConfig, *NestedPermissionConfigPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        const FUnrealAgentValidationResult NestedPermissionValidation =
            FUnrealAgentValidationRunner::RunFastValidation(NestedPermissionDirectory);
        bPassed &= Test.TestFalse(TEXT("Nested agent permission override fails validation"), NestedPermissionValidation.bPassed);

        const FString UnsafeAgentDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitUnsafeAgentTest")));
        IFileManager::Get().DeleteDirectory(*UnsafeAgentDirectory, false, true);
        FUnrealAgentStudioKit::EnsureForProject(UnsafeAgentDirectory);
        const FString UnsafeAgentPath =
            FPaths::Combine(UnsafeAgentDirectory, TEXT(".opencode/agents/unreal-agent.md"));
        const FString UnsafeAgentText = TEXT("---\ndescription: unsafe override\npermission:\n  unreal-engine_*: allow\n---\nUnsafe test agent.\n");
        bPassed &= Test.TestTrue(TEXT("Unsafe agent permission override is seeded"), FFileHelper::SaveStringToFile(UnsafeAgentText, *UnsafeAgentPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        const FUnrealAgentValidationResult UnsafeAgentValidation =
            FUnrealAgentValidationRunner::RunFastValidation(UnsafeAgentDirectory);
        bPassed &= Test.TestFalse(TEXT("Unsafe agent frontmatter permission fails validation"), UnsafeAgentValidation.bPassed);

        IFileManager::Get().DeleteDirectory(*MissingPermissionDirectory, false, true);
        IFileManager::Get().DeleteDirectory(*NestedPermissionDirectory, false, true);
        IFileManager::Get().DeleteDirectory(*UnsafeAgentDirectory, false, true);
        return bPassed;
    }
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentStudioKitTestChecks.h"

#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Acp/StudioKit/UnrealAgentStudioKitPrivate.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if PLATFORM_UNIX || PLATFORM_MAC
#include <unistd.h>
#endif

namespace UnrealAgent::AutomationTests
{
    bool RunStudioKitAtomicRollbackChecks(FAutomationTestBase& Test)
    {
        bool bPassed = true;
#if PLATFORM_UNIX || PLATFORM_MAC
        const FString OwnershipRaceOldManagedText =
            TEXT("unreal_agent_studio_kit_version: 1\nOld managed content.\n");
        const FString RollbackFailureDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitRollbackFailureTest")));
        IFileManager::Get().DeleteDirectory(*RollbackFailureDirectory, false, true);
        bPassed &= Test.TestTrue(
            TEXT("Rollback-failure fixture starts from a managed Studio Kit"),
            FUnrealAgentStudioKit::EnsureForProject(RollbackFailureDirectory).WasSuccessful());
        const FString RollbackFailurePath =
            FPaths::Combine(RollbackFailureDirectory, TEXT(".opencode/agents/unreal-agent.md"));
        bPassed &= Test.TestTrue(
            TEXT("Rollback-failure target is made stale but managed"),
            FFileHelper::SaveStringToFile(
                OwnershipRaceOldManagedText,
                *RollbackFailurePath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        bool bCommitFailureInjected = false;
        bool bRollbackFailureInjected = false;
        UnrealAgentStudioKit::GShouldFailStudioKitAtomicWriteForTest =
            [&bCommitFailureInjected, &bRollbackFailureInjected, RollbackFailurePath](
                const FString& Path,
                const UnrealAgentStudioKit::EStudioKitAtomicWriteFailurePoint Point)
            {
                if (Path != RollbackFailurePath)
                {
                    return false;
                }
                if (Point == UnrealAgentStudioKit::EStudioKitAtomicWriteFailurePoint::CommitDirectorySync)
                {
                    bCommitFailureInjected = true;
                    return true;
                }
                if (Point == UnrealAgentStudioKit::EStudioKitAtomicWriteFailurePoint::Rollback)
                {
                    bRollbackFailureInjected = true;
                    return true;
                }
                return false;
            };
        const FUnrealAgentStudioKitResult RollbackFailureResult =
            FUnrealAgentStudioKit::EnsureForProject(RollbackFailureDirectory);
        UnrealAgentStudioKit::GShouldFailStudioKitAtomicWriteForTest = nullptr;
        FString RollbackFailureFinalText;
        bPassed &= Test.TestTrue(TEXT("Rollback-failure fixture injects commit failure"), bCommitFailureInjected);
        bPassed &= Test.TestTrue(TEXT("Rollback-failure fixture injects rollback failure"), bRollbackFailureInjected);
        bPassed &= Test.TestFalse(TEXT("Rollback failure never reports a successful managed update"), RollbackFailureResult.WasSuccessful());
        bPassed &= Test.TestTrue(TEXT("Rollback failure leaves a readable target"), FFileHelper::LoadFileToString(RollbackFailureFinalText, *RollbackFailurePath));
        IFileManager::Get().DeleteDirectory(*RollbackFailureDirectory, false, true);

        const FString HardlinkDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitHardlinkTest")));
        IFileManager::Get().DeleteDirectory(*HardlinkDirectory, false, true);
        bPassed &= Test.TestTrue(
            TEXT("Hardlink fixture starts from a managed Studio Kit"),
            FUnrealAgentStudioKit::EnsureForProject(HardlinkDirectory).WasSuccessful());
        const FString HardlinkTargetPath =
            FPaths::Combine(HardlinkDirectory, TEXT(".opencode/agents/unreal-agent.md"));
        const FString HardlinkAliasPath =
            FPaths::Combine(HardlinkDirectory, TEXT("user-owned-agent-backup.md"));
        bPassed &= Test.TestTrue(
            TEXT("Hardlink target is made stale but managed"),
            FFileHelper::SaveStringToFile(
                OwnershipRaceOldManagedText,
                *HardlinkTargetPath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        bPassed &= Test.TestEqual(
            TEXT("Hardlink fixture is created"),
            link(TCHAR_TO_UTF8(*HardlinkTargetPath), TCHAR_TO_UTF8(*HardlinkAliasPath)),
            0);
        const FUnrealAgentStudioKitResult HardlinkResult =
            FUnrealAgentStudioKit::EnsureForProject(HardlinkDirectory);
        FString HardlinkTargetText;
        FString HardlinkAliasText;
        bPassed &= Test.TestTrue(
            TEXT("Hardlinked managed target is preserved successfully"),
            HardlinkResult.WasSuccessful()
                && HardlinkResult.PreservedPaths.Contains(HardlinkTargetPath));
        bPassed &= Test.TestTrue(TEXT("Hardlinked managed target remains readable"), FFileHelper::LoadFileToString(HardlinkTargetText, *HardlinkTargetPath));
        bPassed &= Test.TestTrue(TEXT("Hardlinked user-owned alias remains readable"), FFileHelper::LoadFileToString(HardlinkAliasText, *HardlinkAliasPath));
        bPassed &= Test.TestEqual(TEXT("Studio Kit does not mutate a multiply-linked managed target"), HardlinkTargetText, OwnershipRaceOldManagedText);
        bPassed &= Test.TestEqual(TEXT("Studio Kit does not mutate user-owned hardlink content"), HardlinkAliasText, OwnershipRaceOldManagedText);
        IFileManager::Get().DeleteDirectory(*HardlinkDirectory, false, true);
#endif
        return bPassed;
    }
}

#endif

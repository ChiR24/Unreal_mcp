#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentStudioKitTestChecks.h"

#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Acp/StudioKit/UnrealAgentStudioKitPrivate.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if PLATFORM_UNIX || PLATFORM_MAC
#include <sys/stat.h>
#endif

namespace UnrealAgent::AutomationTests
{
    bool RunStudioKitAtomicReplacementChecks(FAutomationTestBase& Test)
    {
        bool bPassed = true;
#if PLATFORM_UNIX || PLATFORM_MAC
        const FString OwnershipRaceOldManagedText =
            TEXT("unreal_agent_studio_kit_version: 1\nOld managed content.\n");
        const FString AtomicReplaceDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentStudioKitAtomicReplaceTest")));
        IFileManager::Get().DeleteDirectory(*AtomicReplaceDirectory, false, true);
        bPassed &= Test.TestTrue(
            TEXT("Atomic-replace fixture starts from a managed Studio Kit"),
            FUnrealAgentStudioKit::EnsureForProject(AtomicReplaceDirectory).WasSuccessful());
        const FString AtomicReplacePath =
            FPaths::Combine(AtomicReplaceDirectory, TEXT(".opencode/agents/unreal-agent.md"));
        bPassed &= Test.TestTrue(
            TEXT("Atomic-replace target is made stale but managed"),
            FFileHelper::SaveStringToFile(
                OwnershipRaceOldManagedText,
                *AtomicReplacePath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        struct stat BeforeAtomicReplaceInfo;
        struct stat AfterAtomicReplaceInfo;
        const bool bHasBeforeAtomicReplaceInfo =
            stat(TCHAR_TO_UTF8(*AtomicReplacePath), &BeforeAtomicReplaceInfo) == 0;
        const FUnrealAgentStudioKitResult AtomicReplaceResult =
            FUnrealAgentStudioKit::EnsureForProject(AtomicReplaceDirectory);
        const bool bHasAfterAtomicReplaceInfo =
            stat(TCHAR_TO_UTF8(*AtomicReplacePath), &AfterAtomicReplaceInfo) == 0;
        bPassed &= Test.TestTrue(TEXT("Managed Studio Kit update succeeds through atomic replacement"), AtomicReplaceResult.WasSuccessful());
        bPassed &= Test.TestTrue(TEXT("Atomic-replace target remains stat-able"), bHasBeforeAtomicReplaceInfo && bHasAfterAtomicReplaceInfo);
        if (bHasBeforeAtomicReplaceInfo && bHasAfterAtomicReplaceInfo)
        {
            bPassed &= Test.TestTrue(
                TEXT("Managed Studio Kit update replaces the filesystem object atomically"),
                BeforeAtomicReplaceInfo.st_dev != AfterAtomicReplaceInfo.st_dev
                    || BeforeAtomicReplaceInfo.st_ino != AfterAtomicReplaceInfo.st_ino);
        }
        IFileManager::Get().DeleteDirectory(*AtomicReplaceDirectory, false, true);

        auto VerifyFailedAtomicReplacePreservesOriginal =
            [&Test, &bPassed, &OwnershipRaceOldManagedText](
                const FString& FixtureName,
                const UnrealAgentStudioKit::EStudioKitAtomicWriteFailurePoint FailurePoint)
            {
                const FString FailureDirectory = FPaths::ConvertRelativePathToFull(
                    FPaths::Combine(FPaths::ProjectSavedDir(), FixtureName));
                IFileManager::Get().DeleteDirectory(*FailureDirectory, false, true);
                bPassed &= Test.TestTrue(
                    *FString::Printf(TEXT("%s starts from a managed Studio Kit"), *FixtureName),
                    FUnrealAgentStudioKit::EnsureForProject(FailureDirectory).WasSuccessful());
                const FString FailurePath =
                    FPaths::Combine(FailureDirectory, TEXT(".opencode/agents/unreal-agent.md"));
                bPassed &= Test.TestTrue(
                    *FString::Printf(TEXT("%s target is made stale but managed"), *FixtureName),
                    FFileHelper::SaveStringToFile(
                        OwnershipRaceOldManagedText,
                        *FailurePath,
                        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));

                struct stat BeforeFailureInfo;
                struct stat AfterFailureInfo;
                const bool bHasBeforeFailureInfo =
                    stat(TCHAR_TO_UTF8(*FailurePath), &BeforeFailureInfo) == 0;
                bool bFailureInjected = false;
                UnrealAgentStudioKit::GShouldFailStudioKitAtomicWriteForTest =
                    [&bFailureInjected, FailurePath, FailurePoint](
                        const FString& Path,
                        const UnrealAgentStudioKit::EStudioKitAtomicWriteFailurePoint Point)
                    {
                        if (bFailureInjected || Path != FailurePath || Point != FailurePoint)
                        {
                            return false;
                        }
                        bFailureInjected = true;
                        return true;
                    };
                const FUnrealAgentStudioKitResult FailureResult =
                    FUnrealAgentStudioKit::EnsureForProject(FailureDirectory);
                UnrealAgentStudioKit::GShouldFailStudioKitAtomicWriteForTest = nullptr;

                FString FinalText;
                const bool bHasAfterFailureInfo =
                    stat(TCHAR_TO_UTF8(*FailurePath), &AfterFailureInfo) == 0;
                bPassed &= Test.TestTrue(
                    *FString::Printf(TEXT("%s injects the requested failure"), *FixtureName),
                    bFailureInjected);
                bPassed &= Test.TestFalse(
                    *FString::Printf(TEXT("%s reports the failed managed update"), *FixtureName),
                    FailureResult.WasSuccessful());
                bPassed &= Test.TestTrue(
                    *FString::Printf(TEXT("%s target remains readable"), *FixtureName),
                    FFileHelper::LoadFileToString(FinalText, *FailurePath));
                bPassed &= Test.TestEqual(
                    *FString::Printf(TEXT("%s restores original managed content"), *FixtureName),
                    FinalText,
                    OwnershipRaceOldManagedText);
                bPassed &= Test.TestTrue(
                    *FString::Printf(TEXT("%s preserves original filesystem identity"), *FixtureName),
                    bHasBeforeFailureInfo
                        && bHasAfterFailureInfo
                        && BeforeFailureInfo.st_dev == AfterFailureInfo.st_dev
                        && BeforeFailureInfo.st_ino == AfterFailureInfo.st_ino);
                IFileManager::Get().DeleteDirectory(*FailureDirectory, false, true);
            };

        VerifyFailedAtomicReplacePreservesOriginal(
            TEXT("UnrealAgentStudioKitCommitSyncFailureTest"),
            UnrealAgentStudioKit::EStudioKitAtomicWriteFailurePoint::CommitDirectorySync);
        VerifyFailedAtomicReplacePreservesOriginal(
            TEXT("UnrealAgentStudioKitBackupRemovalFailureTest"),
            UnrealAgentStudioKit::EStudioKitAtomicWriteFailurePoint::BackupRemoval);
#endif
        return bPassed;
    }
}

#endif

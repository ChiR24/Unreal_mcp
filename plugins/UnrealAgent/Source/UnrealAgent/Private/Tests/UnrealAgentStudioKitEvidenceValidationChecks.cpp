#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/UnrealAgentStudioKitTestChecks.h"

#include "Acp/Evidence/UnrealAgentEvidenceLedger.h"
#include "Acp/StudioKit/UnrealAgentStudioKit.h"
#include "Acp/Validation/UnrealAgentValidationRunner.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgent::AutomationTests
{
    bool RunStudioKitEvidenceValidationChecks(
        FAutomationTestBase& Test,
        const FString& TestDirectory)
    {
        bool bPassed = true;
        const FUnrealAgentValidationResult ValidationResult =
            FUnrealAgentValidationRunner::RunFastValidation(TestDirectory);
        bPassed &= Test.TestTrue(TEXT("Fast validation passes after Studio Kit generation"), ValidationResult.bPassed);
        bPassed &= Test.TestTrue(TEXT("Validation records evidence"), FPaths::FileExists(ValidationResult.EvidencePath));

        FString FirstEvidencePath;
        FString SecondEvidencePath;
        bPassed &= Test.TestTrue(TEXT("First same-type evidence event records"), FUnrealAgentEvidenceLedger::RecordEvent(TestDirectory, TEXT("collision"), TEXT("passed"), TEXT("same summary"), TEXT("details"), &FirstEvidencePath));
        bPassed &= Test.TestTrue(TEXT("Second same-type evidence event records"), FUnrealAgentEvidenceLedger::RecordEvent(TestDirectory, TEXT("collision"), TEXT("passed"), TEXT("same summary"), TEXT("details"), &SecondEvidencePath));
        bPassed &= Test.TestTrue(TEXT("Same-second evidence events use distinct paths"), !FirstEvidencePath.IsEmpty() && !SecondEvidencePath.IsEmpty() && FirstEvidencePath != SecondEvidencePath && FPaths::FileExists(FirstEvidencePath) && FPaths::FileExists(SecondEvidencePath));

        const FString MissingDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentMissingValidationTest")));
        IFileManager::Get().DeleteDirectory(*MissingDirectory, false, true);
        const FUnrealAgentValidationResult MissingValidationResult =
            FUnrealAgentValidationRunner::RunFastValidation(MissingDirectory);
        bPassed &= Test.TestFalse(TEXT("Validation fails for missing project directory"), MissingValidationResult.bPassed);
        bPassed &= Test.TestTrue(TEXT("Validation reports missing project directory"), !MissingValidationResult.Errors.IsEmpty() && MissingValidationResult.Errors[0].Contains(TEXT("Project directory does not exist")));
        bPassed &= Test.TestTrue(TEXT("Missing project validation does not write evidence"), MissingValidationResult.EvidencePath.IsEmpty() && !FPaths::DirectoryExists(FPaths::Combine(MissingDirectory, TEXT("Saved/UnrealAgent"))));

        const FString BrokenDecisionsDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentBrokenDecisionsTest")));
        IFileManager::Get().DeleteDirectory(*BrokenDecisionsDirectory, false, true);
        FUnrealAgentEvidenceLedger::EnsureLedger(BrokenDecisionsDirectory);
        const FString BrokenDecisionsPath =
            FPaths::Combine(BrokenDecisionsDirectory, TEXT("Saved/UnrealAgent/decisions.md"));
        IFileManager::Get().Delete(*BrokenDecisionsPath);
        IFileManager::Get().MakeDirectory(*BrokenDecisionsPath, true);
        Test.AddExpectedErrorPlain(TEXT("UnrealAgentBrokenDecisionsTest"), EAutomationExpectedErrorFlags::Contains, 2);
        FString BrokenDecisionsEvidencePath = TEXT("stale-evidence-path");
        bPassed &= Test.TestFalse(TEXT("Evidence recording fails when decisions ledger cannot be appended"), FUnrealAgentEvidenceLedger::RecordEvent(BrokenDecisionsDirectory, TEXT("broken-decisions"), TEXT("failed"), TEXT("summary"), TEXT("details"), &BrokenDecisionsEvidencePath));
        bPassed &= Test.TestTrue(TEXT("Decision write failure clears stale success path"), BrokenDecisionsEvidencePath.IsEmpty());

        const FString BrokenStateDirectory = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealAgentBrokenStateTest")));
        IFileManager::Get().DeleteDirectory(*BrokenStateDirectory, false, true);
        FUnrealAgentEvidenceLedger::EnsureLedger(BrokenStateDirectory);
        const FString BrokenStatePath =
            FPaths::Combine(BrokenStateDirectory, TEXT("Saved/UnrealAgent/state.json"));
        IFileManager::Get().Delete(*BrokenStatePath);
        IFileManager::Get().MakeDirectory(*BrokenStatePath, true);
        Test.AddExpectedErrorPlain(TEXT("UnrealAgentBrokenStateTest"), EAutomationExpectedErrorFlags::Contains, 2);
        FString BrokenStateEvidencePath = TEXT("stale-evidence-path");
        bPassed &= Test.TestFalse(TEXT("Evidence recording fails when state ledger cannot be written"), FUnrealAgentEvidenceLedger::RecordEvent(BrokenStateDirectory, TEXT("broken-state"), TEXT("failed"), TEXT("summary"), TEXT("details"), &BrokenStateEvidencePath));
        bPassed &= Test.TestTrue(TEXT("State write failure clears stale success path"), BrokenStateEvidencePath.IsEmpty());

        IFileManager::Get().DeleteDirectory(*BrokenDecisionsDirectory, false, true);
        IFileManager::Get().DeleteDirectory(*BrokenStateDirectory, false, true);
        return bPassed;
    }
}

#endif

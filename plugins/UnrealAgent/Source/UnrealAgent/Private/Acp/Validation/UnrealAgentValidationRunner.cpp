#include "Acp/Validation/UnrealAgentValidationRunner.h"

#include "Acp/Context/UnrealAgentEditorContext.h"
#include "Acp/Evidence/UnrealAgentEvidenceLedger.h"
#include "Acp/Validation/UnrealAgentStudioKitValidationChecks.h"

#include "Misc/Paths.h"

namespace
{
    FString NormalizeValidationProjectDirectory(const FString& ProjectDirectory)
    {
        FString Normalized = FPaths::ConvertRelativePathToFull(ProjectDirectory.IsEmpty() ? FPaths::ProjectDir() : ProjectDirectory);
        FPaths::NormalizeDirectoryName(Normalized);
        return Normalized;
    }

    FString JoinLines(const TArray<FString>& Lines)
    {
        FString Text;
        for (const FString& Line : Lines)
        {
            Text += TEXT("- ");
            Text += Line;
            Text += LINE_TERMINATOR;
        }
        return Text;
    }
}

FUnrealAgentValidationResult FUnrealAgentValidationRunner::RunFastValidation(const FString& ProjectDirectory)
{
    FUnrealAgentValidationResult Result;
    const FString NormalizedProjectDirectory = NormalizeValidationProjectDirectory(ProjectDirectory);
    if (!FPaths::DirectoryExists(NormalizedProjectDirectory))
    {
        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(TEXT("Project directory does not exist: %s"), *NormalizedProjectDirectory));
        Result.Summary = TEXT("Validation failed because the project directory does not exist.");
        return Result;
    }

    Result.Checks.Add(FString::Printf(TEXT("OK project directory: %s"), *NormalizedProjectDirectory));

    UnrealAgent::Validation::AddStudioKitValidationChecks(Result, NormalizedProjectDirectory);

    FUnrealAgentEvidenceSummary EvidenceSummary;
    if (FUnrealAgentEvidenceLedger::EnsureLedger(NormalizedProjectDirectory, &EvidenceSummary))
    {
        Result.Checks.Add(FString::Printf(TEXT("OK evidence ledger writable: %s"), *EvidenceSummary.EvidenceDirectory));
    }
    else
    {
        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(TEXT("Evidence ledger is not writable: %s"), *EvidenceSummary.EvidenceDirectory));
    }

    const FUnrealAgentEditorContextSnapshot Context = FUnrealAgentEditorContext::Capture(NormalizedProjectDirectory);
    Result.Checks.Add(Context.Summary);
    if (Context.DirtyPackageCount > 0)
    {
        Result.Warnings.Add(FString::Printf(TEXT("%d dirty packages should be saved or intentionally left dirty before ship checks."), Context.DirtyPackageCount));
    }

    Result.Summary = Result.bPassed
        ? FString::Printf(TEXT("Validation passed with %d checks and %d warnings."), Result.Checks.Num(), Result.Warnings.Num())
        : FString::Printf(TEXT("Validation failed with %d errors and %d warnings."), Result.Errors.Num(), Result.Warnings.Num());

    FString Details;
    Details += TEXT("Checks:\n") + JoinLines(Result.Checks);
    if (!Result.Warnings.IsEmpty())
    {
        Details += TEXT("Warnings:\n") + JoinLines(Result.Warnings);
    }
    if (!Result.Errors.IsEmpty())
    {
        Details += TEXT("Errors:\n") + JoinLines(Result.Errors);
    }

    FString EvidencePath;
    if (FUnrealAgentEvidenceLedger::RecordEvent(
        NormalizedProjectDirectory,
        TEXT("validation"),
        Result.bPassed ? TEXT("passed") : TEXT("failed"),
        Result.Summary,
        Details,
        &EvidencePath))
    {
        Result.EvidencePath = EvidencePath;
    }

    return Result;
}

FString FUnrealAgentValidationRunner::FormatForTranscript(const FUnrealAgentValidationResult& Result)
{
    FString Text = Result.Summary;
    if (!Result.EvidencePath.IsEmpty())
    {
        Text += FString::Printf(TEXT("\nEvidence: %s"), *Result.EvidencePath);
    }
    if (!Result.Errors.IsEmpty())
    {
        Text += TEXT("\nErrors:\n") + JoinLines(Result.Errors);
    }
    if (!Result.Warnings.IsEmpty())
    {
        Text += TEXT("\nWarnings:\n") + JoinLines(Result.Warnings);
    }
    return Text;
}

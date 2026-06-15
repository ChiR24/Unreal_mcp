#include "Acp/StudioKit/UnrealAgentStudioKitPrivate.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgentStudioKit
{
#if WITH_DEV_AUTOMATION_TESTS
TFunction<void(const FString&)> GBeforeStudioKitTemplateWriteForTest;
TFunction<void(const FString&)> GBeforeStudioKitTemplateAtomicWriteForTest;
TFunction<bool(
    const FString&,
    EStudioKitAtomicWriteFailurePoint)>
    GShouldFailStudioKitAtomicWriteForTest;
#endif

bool ShouldFailStudioKitAtomicWriteForTest(
    const FString& Path,
    const EStudioKitAtomicWriteFailurePoint FailurePoint)
{
#if WITH_DEV_AUTOMATION_TESTS
    return GShouldFailStudioKitAtomicWriteForTest
        && GShouldFailStudioKitAtomicWriteForTest(Path, FailurePoint);
#else
    return false;
#endif
}

FString NormalizeStudioKitProjectDirectory(
    const FString& ProjectDirectory)
{
    FString Normalized = FPaths::ConvertRelativePathToFull(
        ProjectDirectory.IsEmpty()
            ? FPaths::ProjectDir()
            : ProjectDirectory);
    FPaths::NormalizeDirectoryName(Normalized);
    return Normalized;
}

ETemplateWriteResult WriteTemplateFileAtomically(
    const FString& ProjectDirectory,
    const FStudioKitTemplateFile& TemplateFile,
    const FString& Path,
    const FString* ExpectedExistingText)
{
#if PLATFORM_UNIX || PLATFORM_MAC
    return WriteStudioKitTemplateUnix(
        ProjectDirectory,
        TemplateFile,
        Path,
        ExpectedExistingText);
#elif PLATFORM_WINDOWS
    return WriteStudioKitTemplateWindows(
        ProjectDirectory,
        TemplateFile,
        Path,
        ExpectedExistingText);
#else
    return ETemplateWriteResult::Failed;
#endif
}

bool LooksLikeLegacyManagedPrompt(const FString& ExistingText)
{
    return ExistingText.Contains(
            TEXT("description: Unreal Editor specialist with live MCP control"))
        || ExistingText.Contains(
            TEXT("description: Unreal Editor game production director with live MCP control"))
        || ExistingText.Contains(
            TEXT("Use the connected unreal-engine MCP tools"))
        || ExistingText.Contains(PromptVersionMarker);
}

void MarkPreserved(
    FUnrealAgentStudioKitResult& Result,
    const FString& Path)
{
    ++Result.FilesPreserved;
    Result.PreservedPaths.Add(Path);
}

void MarkFailed(
    FUnrealAgentStudioKitResult& Result,
    const FString& Path)
{
    ++Result.FilesFailed;
    Result.FailedPaths.Add(Path);
}

bool WriteTemplateFile(
    const FString& ProjectDirectory,
    const FStudioKitTemplateFile& TemplateFile,
    FUnrealAgentStudioKitResult& Result)
{
    const FString Path =
        FPaths::Combine(ProjectDirectory, TemplateFile.RelativePath);
    if (!IsTemplatePathFreeOfLinks(ProjectDirectory, Path))
    {
        MarkFailed(Result, Path);
        return false;
    }
#if WITH_DEV_AUTOMATION_TESTS
    if (GBeforeStudioKitTemplateWriteForTest)
    {
        GBeforeStudioKitTemplateWriteForTest(Path);
    }
#endif
    const bool bIsOpenCodeConfig =
        TemplateFile.RelativePath == TEXT(".opencode/opencode.json");
    FString ExistingText;
    const bool bExistingFile = FPaths::FileExists(Path);
    if (bExistingFile
        && !FFileHelper::LoadFileToString(ExistingText, *Path))
    {
        MarkFailed(Result, Path);
        return false;
    }
    if (bExistingFile)
    {
        if (ExistingText == TemplateFile.Content)
        {
            MarkPreserved(Result, Path);
            return true;
        }

        const bool bManaged =
            FUnrealAgentStudioKit::IsManagedFileText(ExistingText)
            || (bIsOpenCodeConfig
                && ExistingText.Contains(
                    TEXT("\"unreal_agent_studio_kit_version\"")))
            || (bIsOpenCodeConfig
                && LooksLikeLegacyOpenCodeConfig(ExistingText))
            || (TemplateFile.bOverwriteLegacyPrompt
                && LooksLikeLegacyManagedPrompt(ExistingText));
        if (!bManaged)
        {
            MarkPreserved(Result, Path);
            return true;
        }
    }

#if WITH_DEV_AUTOMATION_TESTS
    if (GBeforeStudioKitTemplateAtomicWriteForTest)
    {
        GBeforeStudioKitTemplateAtomicWriteForTest(Path);
    }
#endif
    const ETemplateWriteResult WriteResult =
        WriteTemplateFileAtomically(
            ProjectDirectory,
            TemplateFile,
            Path,
            bExistingFile ? &ExistingText : nullptr);
    if (WriteResult == ETemplateWriteResult::OwnershipChanged)
    {
        if (FPaths::FileExists(Path))
        {
            MarkPreserved(Result, Path);
            return true;
        }
        MarkFailed(Result, Path);
        return false;
    }
    if (WriteResult != ETemplateWriteResult::Written)
    {
        MarkFailed(Result, Path);
        return false;
    }

    ++Result.FilesWritten;
    Result.WrittenPaths.Add(Path);
    return true;
}
}

FUnrealAgentStudioKitResult FUnrealAgentStudioKit::EnsureForProject(
    const FString& ProjectDirectory)
{
    FUnrealAgentStudioKitResult Result;
    const FString NormalizedProjectDirectory =
        UnrealAgentStudioKit::NormalizeStudioKitProjectDirectory(
            ProjectDirectory);
    if (NormalizedProjectDirectory.IsEmpty()
        || !IFileManager::Get().MakeDirectory(
            *NormalizedProjectDirectory,
            true))
    {
        UnrealAgentStudioKit::MarkFailed(
            Result,
            NormalizedProjectDirectory);
        Result.Summary = BuildStatusSummary(Result);
        return Result;
    }

    for (const UnrealAgentStudioKit::FStudioKitTemplateFile& TemplateFile
        : UnrealAgentStudioKit::BuildTemplateFiles())
    {
        UnrealAgentStudioKit::WriteTemplateFile(
            NormalizedProjectDirectory,
            TemplateFile,
            Result);
    }

    Result.Summary = BuildStatusSummary(Result);
    return Result;
}

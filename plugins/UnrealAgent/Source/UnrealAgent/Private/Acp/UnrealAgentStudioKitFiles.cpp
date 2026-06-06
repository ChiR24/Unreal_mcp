#include "UnrealAgentStudioKitPrivate.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgentStudioKit
{
    FString NormalizeStudioKitProjectDirectory(const FString& ProjectDirectory)
    {
        FString Normalized = FPaths::ConvertRelativePathToFull(ProjectDirectory.IsEmpty() ? FPaths::ProjectDir() : ProjectDirectory);
        FPaths::NormalizeDirectoryName(Normalized);
        return Normalized;
    }

    bool EnsureParentDirectory(const FString& Path)
    {
        const FString ParentDirectory = FPaths::GetPath(Path);
        return IFileManager::Get().MakeDirectory(*ParentDirectory, true);
    }

    bool LooksLikeLegacyManagedPrompt(const FString& ExistingText)
    {
        return ExistingText.Contains(TEXT("description: Unreal Editor specialist with live MCP control"))
            || ExistingText.Contains(TEXT("description: Unreal Editor game production director with live MCP control"))
            || ExistingText.Contains(TEXT("Use the connected unreal-engine MCP tools"))
            || ExistingText.Contains(PromptVersionMarker);
    }

    void MarkPreserved(FUnrealAgentStudioKitResult& Result, const FString& Path)
    {
        ++Result.FilesPreserved;
        Result.PreservedPaths.Add(Path);
    }

    void MarkFailed(FUnrealAgentStudioKitResult& Result, const FString& Path)
    {
        ++Result.FilesFailed;
        Result.FailedPaths.Add(Path);
    }

    bool WriteTemplateFile(const FString& ProjectDirectory, const FStudioKitTemplateFile& TemplateFile, FUnrealAgentStudioKitResult& Result)
    {
        const FString Path = FPaths::Combine(ProjectDirectory, TemplateFile.RelativePath);
        const bool bIsOpenCodeConfig = TemplateFile.RelativePath == TEXT(".opencode/opencode.json");
        FString ExistingText;
        if (FPaths::FileExists(Path) && FFileHelper::LoadFileToString(ExistingText, *Path))
        {
            if (ExistingText == TemplateFile.Content)
            {
                MarkPreserved(Result, Path);
                return true;
            }

            const bool bManaged = FUnrealAgentStudioKit::IsManagedFileText(ExistingText)
                || (bIsOpenCodeConfig && ExistingText.Contains(TEXT("\"unreal_agent_studio_kit_version\"")))
                || (bIsOpenCodeConfig && ExistingText == MakeLegacyOpenCodeConfig())
                || (TemplateFile.bOverwriteLegacyPrompt && LooksLikeLegacyManagedPrompt(ExistingText));
            if (!bManaged)
            {
                MarkPreserved(Result, Path);
                return true;
            }
        }

        if (!EnsureParentDirectory(Path)
            || !FFileHelper::SaveStringToFile(TemplateFile.Content, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            MarkFailed(Result, Path);
            return false;
        }

        ++Result.FilesWritten;
        Result.WrittenPaths.Add(Path);
        return true;
    }
}

FUnrealAgentStudioKitResult FUnrealAgentStudioKit::EnsureForProject(const FString& ProjectDirectory)
{
    FUnrealAgentStudioKitResult Result;
    const FString NormalizedProjectDirectory = UnrealAgentStudioKit::NormalizeStudioKitProjectDirectory(ProjectDirectory);
    if (NormalizedProjectDirectory.IsEmpty() || !IFileManager::Get().MakeDirectory(*NormalizedProjectDirectory, true))
    {
        UnrealAgentStudioKit::MarkFailed(Result, NormalizedProjectDirectory);
        Result.Summary = BuildStatusSummary(Result);
        return Result;
    }

    for (const UnrealAgentStudioKit::FStudioKitTemplateFile& TemplateFile : UnrealAgentStudioKit::BuildTemplateFiles())
    {
        UnrealAgentStudioKit::WriteTemplateFile(NormalizedProjectDirectory, TemplateFile, Result);
    }

    Result.Summary = BuildStatusSummary(Result);
    return Result;
}

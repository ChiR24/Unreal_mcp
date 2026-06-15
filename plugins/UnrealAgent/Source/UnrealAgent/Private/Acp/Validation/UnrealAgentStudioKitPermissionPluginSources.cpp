#include "Acp/Validation/UnrealAgentStudioKitPermissionPluginSources.h"

#include "Acp/Validation/UnrealAgentValidationRunner.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UnrealAgent::Validation
{
namespace
{
bool IsOpenCodePluginScript(const FString& Path)
{
    const FString Extension = FPaths::GetExtension(Path).ToLower();
    return Extension == TEXT("ts")
        || Extension == TEXT("js")
        || Extension == TEXT("mjs")
        || Extension == TEXT("cjs");
}
}

void AddOpenCodePluginDirectoryChecks(
    FUnrealAgentValidationResult& Result,
    const FString& Directory,
    const FString& TrustedPluginPath,
    const FString& TrustedPluginSource)
{
    TArray<FString> Files;
    if (FPaths::DirectoryExists(Directory))
    {
        IFileManager::Get().FindFilesRecursive(
            Files,
            *Directory,
            TEXT("*"),
            true,
            false);
    }

    FString NormalizedTrustedPath = TrustedPluginPath;
    FPaths::NormalizeFilename(NormalizedTrustedPath);
    bool bFoundTrustedPlugin = NormalizedTrustedPath.IsEmpty();
    for (FString Path : Files)
    {
        FPaths::NormalizeFilename(Path);
        if (!IsOpenCodePluginScript(Path))
        {
            continue;
        }
        if (!NormalizedTrustedPath.IsEmpty()
            && FPaths::IsSamePath(Path, NormalizedTrustedPath))
        {
            FString ActualSource;
            bFoundTrustedPlugin = FFileHelper::LoadFileToString(ActualSource, *Path)
                && ActualSource == TrustedPluginSource;
            if (!bFoundTrustedPlugin)
            {
                Result.bPassed = false;
                Result.Errors.Add(FString::Printf(
                    TEXT("Unreal Agent guardrail plugin is untrusted or modified: %s"),
                    *Path));
            }
            continue;
        }
        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(
            TEXT("Untrusted OpenCode plugin script is present: %s"),
            *Path));
    }
    if (!bFoundTrustedPlugin)
    {
        Result.bPassed = false;
        Result.Errors.Add(FString::Printf(
            TEXT("Trusted Unreal Agent guardrail plugin is missing: %s"),
            *NormalizedTrustedPath));
    }
}
}

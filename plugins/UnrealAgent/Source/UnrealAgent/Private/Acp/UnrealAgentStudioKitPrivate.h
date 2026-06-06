#pragma once

#include "UnrealAgentStudioKit.h"

namespace UnrealAgentStudioKit
{
    constexpr const TCHAR* StudioKitVersionMarker = TEXT("unreal_agent_studio_kit_version: 1");
    constexpr const TCHAR* PromptVersionMarker = TEXT("unreal_agent_prompt_version: 2");

    struct FStudioKitTemplateFile
    {
        FString RelativePath;
        FString Content;
        bool bOverwriteLegacyPrompt = false;
    };

    void AddTemplate(TArray<FStudioKitTemplateFile>& Templates, const FString& RelativePath, FString Content, bool bOverwriteLegacyPrompt = false);
    void AppendAgentTemplates(TArray<FStudioKitTemplateFile>& Templates);
    void AppendSkillTemplates(TArray<FStudioKitTemplateFile>& Templates);
    void AppendCommandTemplates(TArray<FStudioKitTemplateFile>& Templates);
    void AppendConfigTemplates(TArray<FStudioKitTemplateFile>& Templates);
    TArray<FStudioKitTemplateFile> BuildTemplateFiles();

    FString MakeLegacyOpenCodeConfig();
    bool LooksLikeLegacyManagedPrompt(const FString& ExistingText);
    bool WriteTemplateFile(const FString& ProjectDirectory, const FStudioKitTemplateFile& TemplateFile, FUnrealAgentStudioKitResult& Result);
}

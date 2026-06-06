#include "UnrealAgentStudioKitPrivate.h"

namespace UnrealAgentStudioKit
{
    void AddTemplate(TArray<FStudioKitTemplateFile>& Templates, const FString& RelativePath, FString Content, bool bOverwriteLegacyPrompt)
    {
        Templates.Add({ RelativePath, MoveTemp(Content), bOverwriteLegacyPrompt });
    }

    TArray<FStudioKitTemplateFile> BuildTemplateFiles()
    {
        TArray<FStudioKitTemplateFile> Templates;
        AppendAgentTemplates(Templates);
        AppendSkillTemplates(Templates);
        AppendCommandTemplates(Templates);
        AppendConfigTemplates(Templates);
        return Templates;
    }
}

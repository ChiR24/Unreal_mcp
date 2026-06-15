#include "Acp/Validation/UnrealAgentStudioKitValidationChecks.h"

#include "Acp/Validation/UnrealAgentStudioKitAgentPermissionParser.h"
#include "Acp/Validation/UnrealAgentValidationRunner.h"

#include "HAL/FileManager.h"

namespace UnrealAgent::Validation
{
void AddOpenCodeAgentPermissionChecks(
    FUnrealAgentValidationResult& Result,
    const FString& AgentsDirectory,
    const bool bRequireExplicitPolicy)
{
    TArray<FString> AgentPaths;
    IFileManager::Get().FindFilesRecursive(AgentPaths, *AgentsDirectory, TEXT("*.md"), true, false);
    if (AgentPaths.IsEmpty())
    {
        if (bRequireExplicitPolicy)
        {
            Result.bPassed = false;
            Result.Errors.Add(FString::Printf(
                TEXT("OpenCode agent directory has no agents: %s"),
                *AgentsDirectory));
        }
        return;
    }
    for (const FString& AgentPath : AgentPaths)
    {
        AddAgentFrontMatterPermissionErrors(
            Result,
            AgentPath,
            bRequireExplicitPolicy);
    }
}
}

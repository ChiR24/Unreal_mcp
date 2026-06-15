#pragma once

#include "CoreMinimal.h"

struct FUnrealAgentValidationResult;

namespace UnrealAgent::Validation
{
    void AddAgentFrontMatterPermissionErrors(
        FUnrealAgentValidationResult& Result,
        const FString& AgentPath,
        bool bRequireExplicitPolicy);
}

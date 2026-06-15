#pragma once

#include "CoreMinimal.h"

struct FUnrealAgentValidationResult;

namespace UnrealAgent::Validation
{
    void AddOpenCodePluginDirectoryChecks(
        FUnrealAgentValidationResult& Result,
        const FString& Directory,
        const FString& TrustedPluginPath = FString(),
        const FString& TrustedPluginSource = FString());
}

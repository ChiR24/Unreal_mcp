#pragma once

#include "CoreMinimal.h"

struct FUnrealAgentValidationResult;

namespace UnrealAgent::Validation
{
    void AddOpenCodePermissionCheck(FUnrealAgentValidationResult& Result, const FString& Path);
    void AddOptionalOpenCodePermissionCheck(FUnrealAgentValidationResult& Result, const FString& Path);
    void AddOptionalOpenCodePermissionTextCheck(
        FUnrealAgentValidationResult& Result,
        const FString& ConfigText,
        const FString& Source);
    void AddOpenCodeAgentPermissionChecks(
        FUnrealAgentValidationResult& Result,
        const FString& AgentsDirectory,
        bool bRequireExplicitPolicy = true);
    void AddStudioKitValidationChecks(FUnrealAgentValidationResult& Result, const FString& NormalizedProjectDirectory);
    bool IsProtectedOpenCodePermissionPattern(const FString& Pattern);
    bool ValidateOpenCodePermissionSafety(
        const FString& ProjectDirectory,
        TArray<FString>& OutErrors,
        const TArray<FString>* ManagedConfigDirectoriesOverride = nullptr);
}

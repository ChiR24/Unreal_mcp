#pragma once

#include "CoreMinimal.h"

class FJsonObject;
struct FUnrealAgentValidationResult;

namespace UnrealAgent::Validation
{
    void AddOpenCodePermissionConfigErrors(
        FUnrealAgentValidationResult& Result,
        const TSharedPtr<FJsonObject>& ConfigObject,
        const FString& Source,
        bool bRequireTopLevelPolicy);
}

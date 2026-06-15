#include "Acp/Validation/UnrealAgentStudioKitValidationChecks.h"

namespace UnrealAgent::Validation
{
bool IsProtectedOpenCodePermissionPattern(const FString& Pattern)
{
    const FString TrimmedPattern = Pattern.TrimStartAndEnd();
    return !TrimmedPattern.Equals(TEXT("skill"), ESearchCase::IgnoreCase)
        && !TrimmedPattern.Equals(TEXT("task"), ESearchCase::IgnoreCase);
}
}

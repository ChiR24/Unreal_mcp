#pragma once

#include "CoreMinimal.h"

namespace UnrealAgent::OpenCodeAcp::PermissionBinaryGlob
{
    bool ExpandBracePatterns(
        const FString& Pattern,
        TArray<FString>& OutPatterns);
    bool ShellPatternMayMatchLiteral(
        const FString& Pattern,
        const FString& Literal);
}

#pragma once

#include "CoreMinimal.h"

namespace UnrealAgent::OpenCodeAcp::PermissionSemantics
{
    bool HasInterpreterSemanticMutation(
        const FString& LowerValue,
        const FString& CompactValue);
    bool HasShellPathExpansionMutation(const FString& LowerValue);
    bool HasUnrealSemanticMutation(const FString& Value);
}

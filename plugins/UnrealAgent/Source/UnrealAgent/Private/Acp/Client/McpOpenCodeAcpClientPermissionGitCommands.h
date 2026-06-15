#pragma once

#include "CoreMinimal.h"

namespace UnrealAgent::OpenCodeAcp::PermissionGitCommands
{
    bool ContainsDestructiveGitCommand(const FString& Command);
    bool ContainsGitApplyCommand(const FString& Command);
}

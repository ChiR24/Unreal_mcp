#pragma once

#include "CoreMinimal.h"

namespace UnrealAgent::OpenCodeAcp::PermissionShellMutation
{
    bool ContainsCommandSubstitutionMutation(const FString& Command);
    bool ContainsDestructiveInterpreterOperation(const FString& LowerCommand);
    bool ContainsDynamicShellMutation(const FString& Command);
}

namespace UnrealAgent::OpenCodeAcp
{
    // Lives in this header (and defined in PermissionShellMutation.cpp) because
    // its only callers are the indirect-local-mutation and destructive-local-
    // command detection in PermissionMutation.cpp, which already includes this
    // header. The parent namespace keeps the symbol alongside the other
    // shell-mutation helpers in its implementation file.
    FString NormalizeShellForSafety(FString Command);
}

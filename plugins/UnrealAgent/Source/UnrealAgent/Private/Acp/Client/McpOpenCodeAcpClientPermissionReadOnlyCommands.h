#pragma once

#include "CoreMinimal.h"

namespace UnrealAgent::OpenCodeAcp::PermissionReadOnlyCommands
{
    bool ContainsExecutionCapableReadOption(const FString& Command);
    bool IsExplicitReadOnlyCommandText(const FString& Value);
}

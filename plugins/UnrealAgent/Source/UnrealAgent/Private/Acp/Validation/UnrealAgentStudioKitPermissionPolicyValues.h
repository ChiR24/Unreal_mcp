#pragma once

#include "CoreMinimal.h"

class FJsonValue;

namespace UnrealAgent::Validation::PermissionPolicyValues
{
    bool ConfiguresExternalRuntime(const TSharedPtr<FJsonValue>& Value);
    bool LegacyToolValueEnablesAccess(const TSharedPtr<FJsonValue>& Value);
    bool PermissionValueContainsAllow(const TSharedPtr<FJsonValue>& Value);
}

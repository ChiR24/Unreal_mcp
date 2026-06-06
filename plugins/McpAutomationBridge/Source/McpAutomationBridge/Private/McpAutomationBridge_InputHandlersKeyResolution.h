#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

namespace McpInputHandlers
{
#if WITH_EDITOR
FKey InputKeyFromName(const FString& KeyName);
#endif
}

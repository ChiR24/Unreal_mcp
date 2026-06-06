#pragma once

#include "Templates/SharedPointer.h"

class FJsonObject;
class UInputAction;
class UInputMappingContext;

namespace McpInputHandlers
{
#if WITH_EDITOR
void AddInputMappingSummary(
    TSharedPtr<FJsonObject> Result,
    const UInputMappingContext* Context,
    const UInputAction* InAction);
#endif
}

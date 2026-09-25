#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

class UBlueprint;

namespace McpBlueprintHandlers {
#if WITH_EDITOR
// Writes an object or array defaultValue (a vector {x,y,z}, a color, an array)
// into a variable that the last compile already put on the generated class:
// the CDO value goes through the same converter set_default uses, and its export
// text becomes the variable's own DefaultValue so later compiles keep it.
bool McpApplyVariableObjectDefault(UBlueprint *Blueprint, FName VarName,
                                   const TSharedPtr<FJsonValue> &Value,
                                   FString &OutError);
#endif
} // namespace McpBlueprintHandlers

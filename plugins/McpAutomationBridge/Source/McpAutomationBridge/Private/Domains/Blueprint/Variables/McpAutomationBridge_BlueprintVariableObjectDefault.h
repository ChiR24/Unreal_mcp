#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

class UBlueprint;

namespace McpBlueprintHandlers {
// Writes a defaultValue into a variable that the last compile already put on the
// generated class. A scalar is the variable's export text, checked by importing it
// into the class default object the way the compiler does (the compiler only warns
// on text it cannot parse and keeps the zero value). An object or array (a vector
// {x,y,z}, a color, a list) goes through the converter set_default uses, and its
// export text becomes the variable's DefaultValue so later compiles keep it.
// False, with OutError, when the property cannot take the value.
bool McpApplyVariableDefault(UBlueprint *Blueprint, FName VarName,
                             const TSharedPtr<FJsonValue> &Value,
                             FString &OutError);
} // namespace McpBlueprintHandlers

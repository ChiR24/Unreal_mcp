#pragma once

#include "CoreMinimal.h"

#include "Foundation/BridgeHelpers/Blueprints/McpBPCompileHelpers.h"

// McpSafeCompileBlueprint with the compiler's first error in OutError, so a
// refusal can say why instead of guessing. True when the Blueprint compiled.
static inline bool McpCompileBlueprintWithError(UBlueprint *Blueprint, FString &OutError) {
  OutError.Reset();
  return McpSafeCompileBlueprint(Blueprint, &OutError);
}

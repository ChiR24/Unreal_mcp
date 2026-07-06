#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FMcpToolDefinition;

namespace McpNativeArgumentValidation {
bool ValidateToolArguments(const FMcpToolDefinition *ToolDefinition,
                           const TSharedPtr<FJsonObject> &Arguments,
                           FString &OutArgumentPath, FString &OutErrorCode,
                           FString &OutErrorMessage);
}

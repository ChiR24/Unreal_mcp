#pragma once

#include "CoreMinimal.h"

namespace McpHandlerUtils
{
/**
 * Writes [Section] Key=Value into the project's Default<ConfigName>.ini (ConfigName: Engine, Game,
 * Input, ...), the file the engine reads at startup and packages with the game, then proves the
 * write by reading that file back from disk. OutFile is the absolute file path. Returns false with
 * OutError when the value is not on disk afterwards (read-only file, a flush that wrote nothing).
 */
bool WriteProjectConfigValue(const FString& Section, const FString& Key, const FString& Value,
    const FString& ConfigName, FString& OutFile, FString& OutError);
}

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

struct FProjectConfigEntry
{
    FString Section;
    FString Key;
    FString Value;
};

/**
 * All-or-nothing WriteProjectConfigValue over several entries of one Default<ConfigName>.ini. When
 * one write fails, the entries already written are put back as they were on disk (their old value,
 * or no entry at all) and OutError says whether that restore held. OutWritten lists
 * "[Section] Key=Value" for every entry written; it is empty after a failure.
 */
bool WriteProjectConfigValues(const TArray<FProjectConfigEntry>& Entries, const FString& ConfigName,
    FString& OutFile, TArray<FString>& OutWritten, FString& OutError);
}

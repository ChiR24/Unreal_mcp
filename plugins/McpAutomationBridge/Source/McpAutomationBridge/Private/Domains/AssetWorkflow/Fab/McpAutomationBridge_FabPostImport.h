// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
#include "McpFabProvider.h"

/**
 * What the core does with a Fab import once the adapter has seen it settle.
 *
 * The adapter module owns the wait but cannot save or move assets: the safe wrappers live here. It
 * calls Run with everything the import created, on the game thread, before it stores the outcome, and
 * Run reports what it did by editing the result.
 */
namespace McpFabPostImport
{
/**
 * Saves the packages the import left dirty, through McpSafeAssetSave, and records the count and the
 * names of any it could not save.
 */
void Run(FMcpFabAddResult& Result, const TArray<FString>& ImportedPaths);
} // namespace McpFabPostImport

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
 * Relocates the import when the add named a destinationPath or an assetName (McpFabRelocate), then saves
 * the packages it left dirty, at the paths they have by then, through McpSafeAssetSave, and records the
 * count and the names of any it could not save. DestinationFolder and AssetName are empty when not asked for.
 */
void Run(FMcpFabAddResult& Result, const TArray<FString>& ImportedPaths, const FString& DestinationFolder,
	const FString& AssetName);
} // namespace McpFabPostImport

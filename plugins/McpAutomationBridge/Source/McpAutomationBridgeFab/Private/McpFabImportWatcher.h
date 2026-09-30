// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
#include "McpFabProvider.h"

namespace McpFabImportWatcher
{
/**
 * Watches the asset registry for the import an accepted add started, then records how it ended.
 *
 * The caller has already been answered by the time this runs: it works on its own ticker, reports
 * progress to McpFabImportOperations as it goes, and stores the outcome there under OperationId once
 * the registry settles, Fab logs a failure, or the ceiling is reached.
 */
void WatchForImport(
	const FString& OperationId,
	TSet<FString> Before,
	FMcpFabAddResult Accepted);
} // namespace McpFabImportWatcher

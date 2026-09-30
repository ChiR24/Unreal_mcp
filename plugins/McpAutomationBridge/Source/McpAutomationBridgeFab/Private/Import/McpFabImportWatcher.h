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
 * the registry settles, Fab logs a failure, or the ceiling is reached. PostImport, when set, runs first
 * on whatever landed (see FMcpFabAddOptions::PostImport).
 */
void WatchForImport(
	const FString& OperationId,
	TSet<FString> Before,
	FMcpFabAddResult Accepted,
	TFunction<void(FMcpFabAddResult&, const TArray<FString>&)> PostImport);
} // namespace McpFabImportWatcher

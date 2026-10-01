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
 * the registry settles and Interchange is idle, Fab logs a failure, or the ceiling is reached. PostImport,
 * when set, runs first on whatever landed (see FMcpFabAddOptions::PostImport); SaveAgain, when set, runs on
 * the paths it left 15 and 60 seconds after the outcome is stored (see FMcpFabAddOptions::SaveAgain).
 */
void WatchForImport(
	const FString& OperationId,
	TSet<FString> Before,
	FMcpFabAddResult Accepted,
	TFunction<void(FMcpFabAddResult&, TArray<FString>&)> PostImport,
	TFunction<void(FMcpFabAddResult&, const TArray<FString>&)> SaveAgain);
} // namespace McpFabImportWatcher

// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
#include "McpFabProvider.h"

namespace McpFabImportWatcher
{
/** True while an add's post-accept import is still being observed. */
bool IsBusy();

/** Marks the whole add busy, not just the page call: the import outlives the dispatch slot. */
void SetBusy(bool bBusy);

/**
 * Watches the asset registry until it stops growing, then reports the difference.
 *
 * Completion runs once, on the game thread, after the registry settles or the
 * wait times out. Clears the busy flag itself.
 */
void WatchForImport(
	TSet<FString> Before,
	FMcpFabAddResult Partial,
	TFunction<void(const FMcpFabAddResult&)> OnComplete);
} // namespace McpFabImportWatcher

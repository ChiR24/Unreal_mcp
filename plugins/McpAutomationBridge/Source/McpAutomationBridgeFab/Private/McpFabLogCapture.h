// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

/**
 * Collects the errors Fab logs while an import runs.
 *
 * Fab's workflows report a failed download, unzip or import only through LogFab and then quietly
 * cancel, so the asset registry -- the one other thing this adapter can watch -- simply stays empty
 * and a dead import looks like a slow one until the ceiling. The error lines are what let it say so.
 */
namespace McpFabLogCapture
{
/** Starts collecting LogFab errors, dropping anything left from an earlier run. */
void Start();

/** Stops collecting. Safe to call when not started. */
void Stop();

/** Moves the error lines collected since the last call into OutLines. */
void Take(TArray<FString>& OutLines);

/** True when Line says a Fab workflow gave up: a download, unzip or import failed. */
bool IsWorkflowFailure(const FString& Line);
} // namespace McpFabLogCapture

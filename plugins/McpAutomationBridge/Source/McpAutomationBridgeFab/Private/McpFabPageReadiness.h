// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

/**
 * Whether the Fab tab's page can be scripted yet.
 *
 * The first Fab call after the tab auto-opens finds a browser that is still showing Fab's local
 * bootstrap page or loading fab.com. Scripting it then fails, so the dispatcher waits for the page
 * instead of handing the caller a "retry in a few seconds".
 */
namespace McpFabPageReadiness
{
struct FPageState
{
	/** False when there is no Fab tab and none could be opened. */
	bool bTabFound = false;
	/** The page is on fab.com or www.fab.com. */
	bool bUrlIsFab = false;
	/** The browser is still loading a page. */
	bool bLoading = false;
	/** Why no tab was found, for the refusal that names it. */
	FString Diagnostic;
};

/** Looks at the Fab tab, opening it first when it is not there. */
FPageState Probe();

/** True for a URL on fab.com or www.fab.com, the host compared exactly. */
bool IsFabUrl(const FString& Url);
} // namespace McpFabPageReadiness

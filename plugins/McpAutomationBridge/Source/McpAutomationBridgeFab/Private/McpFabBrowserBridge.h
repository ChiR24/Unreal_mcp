// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

/**
 * How ready Fab's page is to be asked for anything.
 *
 * Encoded as a state rather than handled with a retry sleep because the failure
 * is not rare and not cosmetic: probing two seconds after the tab appears
 * reports about:blank with no frames, and a fetch from that opaque origin fails
 * with "Failed to fetch". A caller that only knew "it didn't work" would retry
 * blindly; a caller told NAVIGATING knows to wait, and one told NOT_FOUND knows
 * to ask the user to open the tab.
 */
enum class EMcpFabBridgeState : uint8
{
	/** No Fab tab is open in this editor session. */
	NotFound,
	/** The tab exists but holds no browser widget. */
	TabFound,
	/** A browser widget exists but has not reached the Fab origin. */
	Navigating,
	/** On https://www.fab.com but window.ue.fab is not published yet. */
	FabOriginReady,
	/** Bindings exist but addtoproject is not callable. */
	FabBindingsReady,
	/** Origin, bindings and addtoproject all confirmed. */
	Ready,
};

const TCHAR* McpFabBridgeStateToString(EMcpFabBridgeState State);

/**
 * The only way anything in this plugin talks to Fab's page.
 *
 * Callers name an operation; they never supply script, and never supply a URL
 * or path that reaches the page. Endpoints are built in native code so that a
 * caller-controlled string cannot steer a request at /i/account or /i/auth.
 */
namespace McpFabBrowserBridge
{
/** Asks the live page what state it is in. Completion runs on the game thread. */
void QueryState(TFunction<void(EMcpFabBridgeState, const FString& /*Detail*/)> Completion);
}

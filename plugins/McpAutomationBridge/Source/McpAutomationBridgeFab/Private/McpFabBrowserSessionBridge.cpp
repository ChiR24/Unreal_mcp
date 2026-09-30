// Copyright (c) 2024 MCP Automation Bridge Contributors

// Reaching Fab's authenticated session without touching its authentication.
//
// Fab's EOS token is unreachable by design: FabAuthentication::GetAuthToken and
// AuthHandle live in Private/ with no FAB_API, so nothing outside the Fab module
// links them. Standing up a second EOS login would work and is the wrong trade --
// two login states, two refresh lifecycles, and an account that can silently
// disagree with the one the user signed into.
//
// The page Fab already authenticated is reachable through entirely public API.
// Fab builds an SWebBrowser inside a nomad tab, and SWebBrowser exports
// ExecuteJavascript and BindUObject (WEBBROWSER_API). So the bridge asks Fab's
// own page to do the privileged work and hand back only the result. MCP never
// holds the token, never logs it, and never serializes it -- it does not possess
// it at all.
//
// This file is the inspection half. It reports what is actually in the tab
// rather than assuming a hierarchy, because the structure below SDockTab is
// Fab's private business and can change between engine versions.

#include "CoreMinimal.h"
#include "McpFabBridgeCallback.h"
#include "McpFabBrowserWidgetSearch.h"
#include "SWebBrowser.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBox.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpFabBridge, Log, All);

namespace McpFabBrowserSession
{
/**
 * Fab does not register one fixed tab id. FFabBrowser::MakeNextFabTabId builds
 * it as "Fab%d" from a counter, so the live tab is Fab1, Fab2, ... and a lookup
 * for a literal "FabTab" never matches: every Fab call then answered
 * FAB_NOT_READY with the tab plainly open on screen. Older builds did use the
 * flat name, so try that first and then probe the counter range.
 */
static const FName FabLegacyTabId(TEXT("FabTab"));
static constexpr int32 MaxFabTabIndex = 16;

/**
 * Opens the Fab tab the same way the menu item does, so no human has to click.
 *
 * Fab never registers a nomad tab spawner we could invoke: FFabBrowser builds a
 * fresh "Fab%d" tab each time through CreateNewFabTab, which lives in the
 * plugin's Private folder with no _API export, so it cannot be linked. What
 * IS reachable is the "OpenFabTab" entry Fab adds to MainFrame.MainMenu.Window.
 * Its action is a plain FUIAction, which FToolMenuEntry keeps private, and
 * TryExecuteToolUIAction only fires an FToolUIAction, so calling that on the
 * entry never opened anything. Generating the menu puts the same FUIAction on a
 * public multibox block named after the entry; executing it is exactly what
 * choosing the menu item does.
 *
 * Guarded to a rendering editor: FFabBrowser::OpenTab asserts under -NullRHI,
 * which is why this is not attempted headless.
 */
static bool TryOpenFabTabViaMenu()
{
	if (!FSlateApplication::IsInitialized() || !FApp::CanEverRender() || IsRunningCommandlet())
	{
		return false;
	}
	UToolMenus* ToolMenus = UToolMenus::Get();
	const FName WindowMenu(TEXT("MainFrame.MainMenu.Window"));
	if (!ToolMenus || !ToolMenus->IsMenuRegistered(WindowMenu))
	{
		return false;
	}
	const TSharedRef<SWidget> Widget = ToolMenus->GenerateWidget(WindowMenu, FToolMenuContext());
	if (Widget->GetTypeAsString() != TEXT("SMultiBoxWidget"))
	{
		return false;
	}
	for (const TSharedRef<const FMultiBlock>& Block :
		 StaticCastSharedRef<SMultiBoxWidget>(Widget)->GetMultiBox()->GetBlocks())
	{
		if (Block->GetExtensionHook() == FName(TEXT("OpenFabTab")) && Block->GetDirectActions().Execute())
		{
			UE_LOG(LogMcpFabBridge, Log, TEXT("Opened the Fab tab through the Window > Fab menu action."));
			return true;
		}
	}
	return false;
}

/** The open Fab tab under any id Fab uses, or nullptr when none is live. */
static TSharedPtr<SDockTab> FindLiveFabTab()
{
	const TSharedPtr<SDockTab> Legacy =
		FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId(FabLegacyTabId));
	if (Legacy.IsValid())
	{
		return Legacy;
	}
	for (int32 Index = 1; Index <= MaxFabTabIndex; ++Index)
	{
		const FName Candidate(*FString::Printf(TEXT("Fab%d"), Index));
		const TSharedPtr<SDockTab> Tab =
			FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId(Candidate));
		if (Tab.IsValid())
		{
			return Tab;
		}
	}
	return nullptr;
}

/**
 * Locates the browser inside the open Fab tab.
 *
 * Returns nothing when the tab was never opened this session: FindExistingLiveTab
 * deliberately does not spawn one, because spawning Fab's tab headlessly asserts
 * inside Epic's FFabBrowser::OpenTab under -NullRHI.
 */

TSharedPtr<SWidget> FindFabBrowserWidget(FString& OutDiagnostic)
{
	if (!FSlateApplication::IsInitialized())
	{
		OutDiagnostic = TEXT("Slate is not initialized (headless run).");
		return nullptr;
	}

	TSharedPtr<SDockTab> FabTab = FindLiveFabTab();
	if (!FabTab.IsValid())
	{
		// An already-open Fab window is always better than a new one.
		FString WindowTree;
		if (TSharedPtr<SWidget> Existing = FindFabBrowserInAnyWindow(WindowTree))
		{
			OutDiagnostic = TEXT("Found an existing Fab browser outside the known tab ids.");
			return Existing;
		}
	}
	bool bOpenAttempted = false;
	if (!FabTab.IsValid())
	{
		// Menu first, reflection second -- the reverse of the original order.
		//
		// The reflection route calls OpenInNewTab on a transient
		// NewObject<UFabBrowserApi> that Fab never initialised, and the tab it
		// produces opens EMPTY: a window shell with no browser session behind
		// it. Scripts dispatched into that shell never reply, so the caller
		// waits out the full 90s abandonment window and sees PAGE_TIMED_OUT.
		// The menu entry executes Fab's own FUIAction, which is the path the
		// UI uses and the only one observed to yield a loaded page.
		bOpenAttempted = TryOpenFabTabViaMenu();
		UE_LOG(LogMcpFabBridge, Log, TEXT("Fab tab auto-open (menu): %s"),
			bOpenAttempted ? TEXT("dispatched") : TEXT("no menu entry"));
	}
	if (!FabTab.IsValid() && bOpenAttempted)
	{
		// CreateNewFabTab builds the tab inline, but the browser widget inside it
		// is attached as the layout settles, so give Slate a couple of ticks
		// before deciding the tab is not there.
		for (int32 Attempt = 0; Attempt < 3 && !FabTab.IsValid(); ++Attempt)
		{
			FSlateApplication::Get().Tick();
			FabTab = FindLiveFabTab();
		}
	}
	if (!FabTab.IsValid())
	{
		OutDiagnostic = TEXT("The Fab tab is not open and could not be opened automatically. "
							 "Open Window > Fab and sign in, then retry. "
							 "(Looked for tab ids FabTab and Fab1..Fab16, and for the "
							 "Window > Fab menu action.)");
		return nullptr;
	}

	int32 Visited = 0;
	TSharedPtr<SWidget> Browser;
	FString Tree;
	WalkWidget(FabTab->GetContent(), 0, Visited, Browser, Tree);

	if (!Browser.IsValid())
	{
		UE_LOG(LogMcpFabBridge, Warning,
			TEXT("No browser widget under the Fab tab; the hierarchy was:\n%s"), *Tree);
		OutDiagnostic = FString::Printf(
			TEXT("Walked %d widget(s) under the Fab tab and found no SWebBrowser/SWebBrowserView."),
			Visited);
		return nullptr;
	}
	OutDiagnostic = FString::Printf(
		TEXT("Found %s after walking %d widget(s)."), *Browser->GetTypeAsString(), Visited);
	return Browser;
}
} // namespace McpFabBrowserSession

// ---------------------------------------------------------------------------
// Talking to the page.
// ---------------------------------------------------------------------------

namespace McpFabBrowserSession
{
bool ReadPageState(FString& OutUrl, bool& bOutLoading, FString& OutDiagnostic)
{
	const TSharedPtr<SWidget> Widget = FindFabBrowserWidget(OutDiagnostic);
	if (!Widget.IsValid() || Widget->GetTypeAsString() != TEXT("SWebBrowser"))
	{
		return false;
	}
	const TSharedRef<SWebBrowser> Browser = StaticCastSharedRef<SWebBrowser>(Widget.ToSharedRef());
	OutUrl = Browser->GetUrl();
	bOutLoading = Browser->IsLoading();
	return true;
}

/**
 * Binds the caller's callback into the live page and runs Script against it.
 *
 * Script is always composed in native code. Nothing reaching this function
 * originates from an MCP request, which is why there is no Mcp.Fab.Eval: console
 * commands are reachable through MCP's console_command, so a generic evaluator
 * would let a caller run window.ue.fab.getauthtoken and read the credential back
 * across the boundary this whole design exists to keep it behind.
 */
bool RunScriptWithCallback(
	const FString& Script,
	UMcpFabBridgeCallback* Callback,
	FString& OutDiagnostic)
{
	const TSharedPtr<SWidget> Widget = FindFabBrowserWidget(OutDiagnostic);
	if (!Widget.IsValid())
	{
		return false;
	}
	if (Widget->GetTypeAsString() != TEXT("SWebBrowser"))
	{
		OutDiagnostic = FString::Printf(
			TEXT("Expected SWebBrowser at the tab root, found %s."), *Widget->GetTypeAsString());
		return false;
	}
	const TSharedRef<SWebBrowser> Browser = StaticCastSharedRef<SWebBrowser>(Widget.ToSharedRef());

	// Permanent, despite only one call being in flight at a time.
	//
	// BindUObject and ExecuteJavascript are both asynchronous into the render
	// process, so a non-permanent binding races its own script: when the
	// script wins, window.ue.mcpfab is undefined, the reply call throws into
	// the script's own catch, and nothing ever settles the request -- which
	// surfaces as PAGE_TIMED_OUT after the full abandonment window, with no
	// hint that the binding was the problem.
	//
	// A permanent binding is also re-applied on every navigation, which is
	// what fab.com actually needs: it is a single-page app that routes
	// constantly, and the origin guard above can navigate it too. A
	// non-permanent binding is silently lost at the first route change.
	Browser->BindUObject(TEXT("mcpFab"), Callback, /*bIsPermanent=*/true);
	Browser->ExecuteJavascript(Script);
	OutDiagnostic = TEXT("script dispatched");
	return true;
}
} // namespace McpFabBrowserSession

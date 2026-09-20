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
#include "SWebBrowser.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
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
 * fresh "Fab%d" tab each time and only exposes CreateNewFabTab, which lives in
 * the plugin's Private folder with no _API export, so it cannot be linked. What
 * IS reachable is the ToolMenus entry Fab installs in SetupEntryPoints --
 * "OpenFabTab" under MainFrame.MainMenu.Window, section GetContent -- whose
 * FUIAction calls CreateNewFabTab. Executing that action is exactly what
 * choosing the menu item does.
 *
 * Guarded to a rendering editor: FFabBrowser::OpenTab asserts under -NullRHI,
 * which is why this is not attempted headless.
 */
/** Defined in McpFabDirectApi.cpp: opens the tab with no UI path at all. */
} // namespace McpFabBrowserSession
namespace McpFabDirectApi { bool TryOpenFabTabViaReflection(FString& OutDiagnostic); }
namespace McpFabBrowserSession
{

static bool TryOpenFabTabViaMenu()
{
	if (!FSlateApplication::IsInitialized() || !FApp::CanEverRender() || IsRunningCommandlet())
	{
		return false;
	}
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus)
	{
		return false;
	}
	// The toolbar button carries the same action, so either entry will do.
	const TArray<TPair<FName, FName>> Candidates = {
		{ TEXT("MainFrame.MainMenu.Window"), TEXT("OpenFabTab") },
		{ TEXT("ContentBrowser.Toolbar"), TEXT("OpenFabWindow") },
	};
	for (const TPair<FName, FName>& Candidate : Candidates)
	{
		UToolMenu* Menu = ToolMenus->FindMenu(Candidate.Key);
		if (!Menu)
		{
			continue;
		}
		for (FToolMenuSection& Section : Menu->Sections)
		{
			if (FToolMenuEntry* Entry = Section.FindEntry(Candidate.Value))
			{
				// FToolMenuEntry::Action is private; TryExecuteToolUIAction is the
				// public way to fire the same delegate the menu item fires.
				FToolMenuContext EmptyContext;
				if (Entry->TryExecuteToolUIAction(EmptyContext))
				{
					UE_LOG(LogMcpFabBridge, Log,
						TEXT("Opened the Fab tab through menu entry %s."),
						*Candidate.Value.ToString());
					return true;
				}
			}
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

/** Widget type names that expose ExecuteJavascript/BindUObject. */
static bool IsBrowserWidget(const FString& TypeName)
{
	return TypeName == TEXT("SWebBrowser") || TypeName == TEXT("SWebBrowserView");
}

/**
 * Depth-first walk of the live Slate tree.
 *
 * GetType() and GetChildren() are public on SWidget, so an external module can
 * inspect a hierarchy it did not build. Every visited type is logged so the
 * first run produces the real shape of Fab's tab instead of a guess at it.
 */
static void WalkWidget(
	const TSharedRef<SWidget>& Widget,
	int32 Depth,
	int32& InOutVisited,
	TSharedPtr<SWidget>& OutBrowser,
	FString& OutTree)
{
	// A runaway tree would spam the log and hide the answer; Fab's tab is shallow.
	static constexpr int32 MaxDepth = 40;
	static constexpr int32 MaxWidgets = 4000;
	if (Depth > MaxDepth || InOutVisited >= MaxWidgets)
	{
		return;
	}
	++InOutVisited;

	const FString TypeName = Widget->GetTypeAsString();
	// Accumulated rather than logged as we go: the walk runs on every Fab call,
	// and emitting a line per widget buried each successful search under twenty
	// lines of tree. It is printed only when the browser is not found, which is
	// the only time the shape of the tree is the thing you need to see.
	OutTree += FString::Printf(
		TEXT("%s%s\n"), *FString::ChrN(Depth * 2, TEXT(' ')), *TypeName);

	if (!OutBrowser.IsValid() && IsBrowserWidget(TypeName))
	{
		OutBrowser = Widget;
		OutTree += FString::Printf(TEXT("  ^ browser widget found at depth %d\n"), Depth);
	}

	FChildren* Children = Widget->GetChildren();
	const int32 Count = Children ? Children->Num() : 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		WalkWidget(Children->GetChildAt(Index), Depth + 1, InOutVisited, OutBrowser, OutTree);
	}
}

/**
 * Locates the browser inside the open Fab tab.
 *
 * Returns nothing when the tab was never opened this session: FindExistingLiveTab
 * deliberately does not spawn one, because spawning Fab's tab headlessly asserts
 * inside Epic's FFabBrowser::OpenTab under -NullRHI.
 */
/** OutTree is filled with the walked hierarchy for callers that want to print it. */
TSharedPtr<SWidget> FindFabBrowserWidget(FString& OutDiagnostic, FString* OutTree = nullptr)
{
	if (!FSlateApplication::IsInitialized())
	{
		OutDiagnostic = TEXT("Slate is not initialized (headless run).");
		return nullptr;
	}

	TSharedPtr<SDockTab> FabTab = FindLiveFabTab();
	bool bOpenAttempted = false;
	if (!FabTab.IsValid())
	{
		// Reflection first: it reaches CreateNewFabTab directly and does not
		// depend on ToolMenus having the entry registered yet. The menu route
		// stays as the fallback for Fab versions with no OpenInNewTab UFUNCTION.
		FString OpenDiagnostic;
		bOpenAttempted = McpFabDirectApi::TryOpenFabTabViaReflection(OpenDiagnostic);
		UE_LOG(LogMcpFabBridge, Log, TEXT("Fab tab auto-open (reflection): %s"), *OpenDiagnostic);
		if (!bOpenAttempted)
		{
			bOpenAttempted = TryOpenFabTabViaMenu();
		}
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
							 "OpenFabTab/OpenFabWindow menu entries.)");
		return nullptr;
	}

	int32 Visited = 0;
	TSharedPtr<SWidget> Browser;
	FString Tree;
	WalkWidget(FabTab->GetContent(), 0, Visited, Browser, Tree);
	if (OutTree != nullptr)
	{
		*OutTree = Tree;
	}

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

/**
 * Console probe. Dumps the live Fab tab's widget hierarchy to the log and says
 * whether a browser widget is reachable, which is the one fact the rest of the
 * design depends on. Diagnostic only: it executes no script and binds nothing.
 */
static FAutoConsoleCommand GMcpFabDumpBrowserTree(
	TEXT("Mcp.Fab.DumpBrowserTree"),
	TEXT("Logs the Slate widget hierarchy of the open Fab tab and reports whether a web browser widget was found."),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		FString Diagnostic;
		FString Tree;
		const TSharedPtr<SWidget> Browser =
			McpFabBrowserSession::FindFabBrowserWidget(Diagnostic, &Tree);
		UE_LOG(LogMcpFabBridge, Log, TEXT("Mcp.Fab.DumpBrowserTree:\n%s"), *Tree);
		UE_LOG(LogMcpFabBridge, Log, TEXT("Mcp.Fab.DumpBrowserTree: %s"), *Diagnostic);
		UE_LOG(LogMcpFabBridge, Log, TEXT("Mcp.Fab.DumpBrowserTree: browserReachable=%s"),
			Browser.IsValid() ? TEXT("true") : TEXT("false"));
	}));

// ---------------------------------------------------------------------------
// Talking to the page.
// ---------------------------------------------------------------------------

namespace McpFabBrowserSession
{
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

	// Not permanent: a permanent binding is re-applied on every navigation, and
	// the object only needs to exist for the call in flight.
	Browser->BindUObject(TEXT("mcpFab"), Callback, /*bIsPermanent=*/false);
	Browser->ExecuteJavascript(Script);
	OutDiagnostic = TEXT("script dispatched");
	return true;
}
} // namespace McpFabBrowserSession

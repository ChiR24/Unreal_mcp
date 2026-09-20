#include "McpFabBrowserWidgetSearch.h"

#include "SWebBrowser.h"
#include "SWebBrowserView.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

namespace McpFabBrowserSession
{
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
void WalkWidget(
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

// The Fab browser in ANY live window, preferring one actually showing fab.com.
//
// FindLiveFabTab only probes the tab ids Fab registers today (FabTab, Fab1..16).
// A Fab window opened by the user did not match, so the bridge concluded no tab
// existed and opened its OWN -- which came up blank, because the tab shell is
// spawned from a transient UFabBrowserApi that owns no browser session. Every
// script then went into that empty window and nothing ever replied, which the
// caller saw as PAGE_TIMED_OUT ninety seconds later while a perfectly good
// signed-in Fab window sat next to it.
//
// Searching the widget tree is id-agnostic, so it keeps working when Fab renames
// or renumbers its tabs. The URL check is what makes it pick the user's loaded
// window over a blank one this bridge may have opened earlier; a browser with no
// URL is kept only as a last resort.
TSharedPtr<SWidget> FindFabBrowserInAnyWindow(FString& OutTree)
{
	TSharedPtr<SWidget> Blank;
	for (const TSharedRef<SWindow>& Window : FSlateApplication::Get().GetInteractiveTopLevelWindows())
	{
		int32 Visited = 0;
		TSharedPtr<SWidget> Found;
		WalkWidget(Window, 0, Visited, Found, OutTree);
		if (!Found.IsValid())
		{
			continue;
		}
		const FString Url = Found->GetTypeAsString() == TEXT("SWebBrowser")
			? StaticCastSharedPtr<SWebBrowser>(Found)->GetUrl()
			: StaticCastSharedPtr<SWebBrowserView>(Found)->GetUrl();
		if (Url.Contains(TEXT("fab.com")))
		{
			return Found;
		}
		// Only a browser that has loaded nothing yet can be Fab's tab still
		// coming up. One showing any other site is somebody else's -- the docs
		// or login window -- and scripting into it would navigate it away.
		if (Url.IsEmpty() && !Blank.IsValid())
		{
			Blank = Found;
		}
	}
	return Blank;
}
} // namespace McpFabBrowserSession

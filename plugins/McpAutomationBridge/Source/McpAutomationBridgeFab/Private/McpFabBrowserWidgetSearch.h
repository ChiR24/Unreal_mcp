#pragma once

// Slate-tree search used to reach Fab's embedded browser.
//
// Split out of McpFabBrowserSessionBridge.cpp purely for the 250-pure-line
// source contract: that file sat at 248 and had no room for the window-wide
// search this adds.

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"

namespace McpFabBrowserSession
{
void WalkWidget(const TSharedRef<SWidget>& Widget, int32 Depth, int32& InOutVisited,
	TSharedPtr<SWidget>& OutBrowser, FString& OutTree);

TSharedPtr<SWidget> FindFabBrowserInAnyWindow(FString& OutTree);
} // namespace McpFabBrowserSession

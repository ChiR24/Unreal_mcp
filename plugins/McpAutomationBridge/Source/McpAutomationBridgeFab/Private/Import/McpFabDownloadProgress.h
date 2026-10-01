// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

/**
 * Reads, and presses the Cancel button of, Fab's own download notification.
 *
 * Fab shows "Downloading <name>" with a percent bar for every add it imports. The percent is fed from
 * the download request's progress (the BuildPatch installer for a unreal-engine pack, the HTTP stream
 * for a source format), and the request object itself is private to Fab's workflow -- so the toast is
 * the only place that progress, and the pack workflow's Cancel button, can be reached from outside.
 * Reading a widget is an inspection and pressing its button is what a click does; neither needs a
 * credential or a script. The toast is found by its title, which is "Downloading " plus the name the
 * add handed Fab (the version or file name), so two downloads cannot be mistaken for each other.
 */
namespace McpFabDownloadProgress
{
struct FNotification
{
	bool bFound = false;
	/** 0-100, or -1 while the toast shows no percent yet. */
	float Percent = -1.0f;
	/** The toast has a Cancel button that can be pressed: only a unreal-engine pack's does. */
	bool bCancellable = false;
};

/** What the toast for the download named AssetName shows, if one is up. */
FNotification Read(const FString& AssetName);

/** Presses that toast's Cancel button. False when there is no such toast or it offers no cancel. */
bool PressCancel(const FString& AssetName);
} // namespace McpFabDownloadProgress

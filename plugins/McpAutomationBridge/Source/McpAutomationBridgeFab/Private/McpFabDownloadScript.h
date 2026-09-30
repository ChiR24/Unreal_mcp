// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

namespace McpFabDownload
{
/**
 * The page-side step that turns the chosen file into a signed download address, and says so plainly
 * when Fab will not give one.
 *
 * It is spliced into the add script, where `out`, `base`, `send`, `scrub` and `shape` are in scope:
 * resolveDownload(chosen) answers {url, bases}, or null after reporting NO_DOWNLOAD_URL together with
 * the step that failed (license, claim or download-info), Fab's HTTP status and Fab's own words.
 */
const TCHAR* Script();
} // namespace McpFabDownload

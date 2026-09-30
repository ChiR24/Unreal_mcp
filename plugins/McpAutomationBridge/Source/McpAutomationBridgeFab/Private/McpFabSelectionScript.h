// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

namespace McpFabSelection
{
/**
 * The page-side functions that decide what an add downloads: which engine version of a pack, which
 * file (and quality tier) of a source format, and how big it is.
 *
 * One definition, interpolated into every script that needs it, so the listing details can say what
 * the add would fetch and be right: they cannot drift apart because they are the same code.
 */
const TCHAR* Script();
} // namespace McpFabSelection

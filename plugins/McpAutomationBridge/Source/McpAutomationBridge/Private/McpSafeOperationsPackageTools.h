#pragma once

#if WITH_EDITOR && __has_include("PackageTools.h")
#include "PackageTools.h"
#define MCP_HAS_PACKAGE_TOOLS 1
#else
#define MCP_HAS_PACKAGE_TOOLS 0
#endif

#pragma once

#include "McpVersionCompatibility.h"

#if __has_include("AbilitySystemComponent.h")
#define MCP_HAS_GAS 1
#else
#define MCP_HAS_GAS 0
#endif

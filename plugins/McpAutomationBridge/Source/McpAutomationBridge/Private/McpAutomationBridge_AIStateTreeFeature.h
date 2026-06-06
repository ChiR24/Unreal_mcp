#pragma once

#include "McpVersionCompatibility.h"

#if ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 3
#define MCP_HAS_STATE_TREE 1
#if __has_include("StateTree.h")
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#if __has_include("Components/StateTreeComponentSchema.h")
#include "Components/StateTreeComponentSchema.h"
#define MCP_STATE_TREE_COMPONENT_SCHEMA_AVAILABLE 1
#else
#define MCP_STATE_TREE_COMPONENT_SCHEMA_AVAILABLE 0
#endif
#define MCP_STATE_TREE_HEADERS_AVAILABLE 1
#else
#define MCP_STATE_TREE_HEADERS_AVAILABLE 0
#define MCP_STATE_TREE_COMPONENT_SCHEMA_AVAILABLE 0
#endif
#else
#define MCP_HAS_STATE_TREE 0
#define MCP_STATE_TREE_HEADERS_AVAILABLE 0
#define MCP_STATE_TREE_COMPONENT_SCHEMA_AVAILABLE 0
#endif

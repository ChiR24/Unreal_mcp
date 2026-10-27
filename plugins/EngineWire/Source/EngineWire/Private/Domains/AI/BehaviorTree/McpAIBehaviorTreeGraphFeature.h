#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode_Root.h"
#define MCP_AI_HAS_BEHAVIOR_TREE_GRAPH 1
#else
#define MCP_AI_HAS_BEHAVIOR_TREE_GRAPH 0
#endif

#pragma once

#include "McpVersionCompatibility.h"

#if ENGINE_MAJOR_VERSION >= 5
#define MCP_HAS_SMART_OBJECTS 1
#if __has_include("SmartObjectDefinition.h")
#include "GameplayTagContainer.h"
#include "SmartObjectComponent.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectTypes.h"
#define MCP_SMART_OBJECTS_HEADERS_AVAILABLE 1
#else
#define MCP_SMART_OBJECTS_HEADERS_AVAILABLE 0
#endif
#else
#define MCP_HAS_SMART_OBJECTS 0
#define MCP_SMART_OBJECTS_HEADERS_AVAILABLE 0
#endif

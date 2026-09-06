#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#if defined(__has_include)
#if __has_include("BlueprintGraph/K2Node_CallFunction.h")
#include "BlueprintGraph/K2Node_CallFunction.h"
#include "BlueprintGraph/K2Node_CustomEvent.h"
#include "BlueprintGraph/K2Node_Event.h"
#include "BlueprintGraph/K2Node_FunctionEntry.h"
#include "BlueprintGraph/K2Node_FunctionResult.h"
#include "BlueprintGraph/K2Node_Literal.h"
#include "BlueprintGraph/K2Node_VariableGet.h"
#include "BlueprintGraph/K2Node_VariableSet.h"
#define MCP_HAS_K2NODE_HEADERS 1
#elif __has_include("BlueprintGraph/Classes/K2Node_CallFunction.h")
#include "BlueprintGraph/Classes/K2Node_CallFunction.h"
#include "BlueprintGraph/Classes/K2Node_CustomEvent.h"
#include "BlueprintGraph/Classes/K2Node_Event.h"
#include "BlueprintGraph/Classes/K2Node_FunctionEntry.h"
#include "BlueprintGraph/Classes/K2Node_FunctionResult.h"
#include "BlueprintGraph/Classes/K2Node_Literal.h"
#include "BlueprintGraph/Classes/K2Node_VariableGet.h"
#include "BlueprintGraph/Classes/K2Node_VariableSet.h"
#define MCP_HAS_K2NODE_HEADERS 1
#elif __has_include("K2Node_CallFunction.h")
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Literal.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#define MCP_HAS_K2NODE_HEADERS 1
#else
#define MCP_HAS_K2NODE_HEADERS 0
#endif
#else
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Literal.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#define MCP_HAS_K2NODE_HEADERS 1
#endif

#if defined(__has_include)
#if __has_include("EdGraphSchema_K2.h")
#include "EdGraphSchema_K2.h"
#define MCP_HAS_EDGRAPH_SCHEMA_K2 1
#elif __has_include("BlueprintGraph/EdGraphSchema_K2.h")
#include "BlueprintGraph/EdGraphSchema_K2.h"
#define MCP_HAS_EDGRAPH_SCHEMA_K2 1
#elif __has_include("BlueprintGraph/Classes/EdGraphSchema_K2.h")
#include "BlueprintGraph/Classes/EdGraphSchema_K2.h"
#define MCP_HAS_EDGRAPH_SCHEMA_K2 1
#elif __has_include("EdGraph/EdGraphSchema_K2.h")
#include "EdGraph/EdGraphSchema_K2.h"
#define MCP_HAS_EDGRAPH_SCHEMA_K2 1
#else
#define MCP_HAS_EDGRAPH_SCHEMA_K2 0
#endif
#else
#include "EdGraphSchema_K2.h"
#define MCP_HAS_EDGRAPH_SCHEMA_K2 1
#endif
#else
#define MCP_HAS_K2NODE_HEADERS 0
#define MCP_HAS_EDGRAPH_SCHEMA_K2 0
#endif

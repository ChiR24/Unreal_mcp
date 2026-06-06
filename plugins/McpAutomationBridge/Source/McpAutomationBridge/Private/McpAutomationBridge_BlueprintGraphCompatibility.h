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

#if MCP_HAS_EDGRAPH_SCHEMA_K2
#define MCP_PC_Float UEdGraphSchema_K2::PC_Float
#define MCP_PC_Int UEdGraphSchema_K2::PC_Int
#define MCP_PC_Int64 UEdGraphSchema_K2::PC_Int64
#define MCP_PC_Byte UEdGraphSchema_K2::PC_Byte
#define MCP_PC_Boolean UEdGraphSchema_K2::PC_Boolean
#define MCP_PC_String UEdGraphSchema_K2::PC_String
#define MCP_PC_Name UEdGraphSchema_K2::PC_Name
#define MCP_PC_Object UEdGraphSchema_K2::PC_Object
#define MCP_PC_Class UEdGraphSchema_K2::PC_Class
#define MCP_PC_Wildcard UEdGraphSchema_K2::PC_Wildcard
#define MCP_PC_Text UEdGraphSchema_K2::PC_Text
#define MCP_PC_Struct UEdGraphSchema_K2::PC_Struct
#else
static const FName MCP_PC_Float(TEXT("float"));
static const FName MCP_PC_Int(TEXT("int"));
static const FName MCP_PC_Int64(TEXT("int64"));
static const FName MCP_PC_Byte(TEXT("byte"));
static const FName MCP_PC_Boolean(TEXT("bool"));
static const FName MCP_PC_String(TEXT("string"));
static const FName MCP_PC_Name(TEXT("name"));
static const FName MCP_PC_Object(TEXT("object"));
static const FName MCP_PC_Class(TEXT("class"));
static const FName MCP_PC_Wildcard(TEXT("wildcard"));
static const FName MCP_PC_Text(TEXT("text"));
static const FName MCP_PC_Struct(TEXT("struct"));
#endif

#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"

#if ENGINE_MINOR_VERSION >= 3
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

#if MCP_HAS_STATE_TREE && MCP_STATE_TREE_HEADERS_AVAILABLE
// The first state named Name (case ignored) at or under State; null when none.
inline UStateTreeState* McpFindStateTreeState(UStateTreeState* State, const FString& Name)
{
    if (!State)
    {
        return nullptr;
    }
    if (State->Name.ToString().Equals(Name, ESearchCase::IgnoreCase))
    {
        return State;
    }
    for (UStateTreeState* Child : State->Children)
    {
        if (UStateTreeState* Found = McpFindStateTreeState(Child, Name))
        {
            return Found;
        }
    }
    return nullptr;
}

// The first state named Name anywhere in EditorData's subtrees.
inline UStateTreeState* McpFindStateTreeState(const UStateTreeEditorData* EditorData, const FString& Name)
{
    for (UStateTreeState* SubTree : EditorData->SubTrees)
    {
        if (UStateTreeState* Found = McpFindStateTreeState(SubTree, Name))
        {
            return Found;
        }
    }
    return nullptr;
}
#endif

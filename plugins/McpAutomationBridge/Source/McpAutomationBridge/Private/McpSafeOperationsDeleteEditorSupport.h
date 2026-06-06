#pragma once

#include "McpSafeOperationsAssetEditorSubsystem.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"

#if __has_include("AssetCompilingManager.h")
#include "AssetCompilingManager.h"
#define MCP_HAS_ASSET_COMPILING_MANAGER 1
#else
#define MCP_HAS_ASSET_COMPILING_MANAGER 0
#endif

#if __has_include("Animation/AnimBlueprint.h")
#include "Animation/AnimBlueprint.h"
#define MCP_HAS_ANIM_BLUEPRINT 1
#else
#define MCP_HAS_ANIM_BLUEPRINT 0
#endif

#if __has_include("AnimationEditorUtils.h")
#include "AnimationEditorUtils.h"
#define MCP_HAS_ANIMATION_EDITOR_UTILS 1
#else
#define MCP_HAS_ANIMATION_EDITOR_UTILS 0
#endif

#if __has_include("Engine/Selection.h")
#include "Engine/Selection.h"
#define MCP_HAS_SELECTION 1
#else
#define MCP_HAS_SELECTION 0
#endif

#if __has_include("BlueprintActionDatabase.h")
#include "BlueprintActionDatabase.h"
#define MCP_HAS_BLUEPRINT_ACTION_DATABASE 1
#else
#define MCP_HAS_BLUEPRINT_ACTION_DATABASE 0
#endif
#endif

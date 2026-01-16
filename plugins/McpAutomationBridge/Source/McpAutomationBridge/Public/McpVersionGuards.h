#pragma once

/**
 * McpVersionGuards.h - Centralized UE Version Detection Macros
 * 
 * This header provides compile-time macros for detecting Unreal Engine versions
 * and feature availability. Include this in any source file that needs to
 * conditionally compile code based on UE version or plugin availability.
 * 
 * NOTE: Plugin detection macros (MCP_HAS_*) for optional modules are defined
 * in McpAutomationBridge.Build.cs via PublicDefinitions.Add(). This header
 * defines version-based macros only.
 */

#include "Runtime/Launch/Resources/Version.h"

// ============================================================================
// MINIMUM VERSION REQUIREMENT
// ============================================================================
// MCP Automation Bridge requires Unreal Engine 5.5 or later.
// This check will cause a compile error on unsupported engine versions.
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 5)
#error "MCP Automation Bridge requires Unreal Engine 5.5 or later"
#endif

// ============================================================================
// VERSION GUARD MACROS
// ============================================================================

/**
 * MCP_UE55_PLUS - Defined when compiling for UE 5.5 or later.
 * Use for features introduced in UE 5.5.
 */
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
#define MCP_UE55_PLUS 1
#else
#define MCP_UE55_PLUS 0
#endif

/**
 * MCP_UE56_PLUS - Defined when compiling for UE 5.6 or later.
 * Use for features introduced in UE 5.6.
 */
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
#define MCP_UE56_PLUS 1
#else
#define MCP_UE56_PLUS 0
#endif

/**
 * MCP_UE57_ONLY - Defined when compiling EXCLUSIVELY for UE 5.7.
 * Use for code that should only run on 5.7, not earlier or later versions.
 */
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 7
#define MCP_UE57_ONLY 1
#else
#define MCP_UE57_ONLY 0
#endif

/**
 * MCP_UE57_PLUS - Defined when compiling for UE 5.7 or later.
 * Use for features introduced in UE 5.7.
 */
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
#define MCP_UE57_PLUS 1
#else
#define MCP_UE57_PLUS 0
#endif

// ============================================================================
// UE 5.7+ FEATURE DETECTION MACROS
// ============================================================================

/**
 * MCP_HAS_AI_ASSISTANT - AI Assistant plugin (UE 5.7+)
 */
#if MCP_UE57_PLUS
#define MCP_HAS_AI_ASSISTANT 1
#else
#define MCP_HAS_AI_ASSISTANT 0
#endif

/**
 * MCP_HAS_PCG_GPU - PCG GPU Processing (UE 5.7+)
 * GPU-accelerated procedural content generation.
 */
#if MCP_UE57_PLUS
#define MCP_HAS_PCG_GPU 1
#else
#define MCP_HAS_PCG_GPU 0
#endif

/**
 * MCP_HAS_ANIMATOR_KIT - Animator Kit / Animation Authoring (UE 5.7+)
 * Enhanced animation authoring tools.
 */
#if MCP_UE57_PLUS
#define MCP_HAS_ANIMATOR_KIT 1
#else
#define MCP_HAS_ANIMATOR_KIT 0
#endif

/**
 * MCP_HAS_MEGALIGHTS - MegaLights (UE 5.7+)
 * Virtual shadow maps and improved light rendering.
 */
#if MCP_UE57_PLUS
#define MCP_HAS_MEGALIGHTS 1
#else
#define MCP_HAS_MEGALIGHTS 0
#endif

/**
 * MCP_HAS_SUBSTRATE - Substrate Materials (UE 5.7+)
 * Next-generation material system.
 */
#if MCP_UE57_PLUS
#define MCP_HAS_SUBSTRATE 1
#else
#define MCP_HAS_SUBSTRATE 0
#endif

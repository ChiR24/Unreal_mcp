// McpToolRegistry.h — Singleton registry for self-describing MCP tool definitions

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpToolDefinition;

/**
 * Global registry of MCP tool definitions.
 *
 * Tool .cpp files register via FMcpToolAutoRegistrar at static init time.
 * The transport queries this at runtime for tools/list and dispatch metadata.
 *
 * Thread safety: CacheMutex protects Tools, ToolsByName, CachedToolSchemas,
 * and bCacheValid. Register() and all read operations acquire CacheMutex.
 */
class FMcpToolRegistry
{
public:
	/** Get the singleton instance (Meyer's singleton). */
	static FMcpToolRegistry& Get();

	/** Register a tool definition. Called at static init time by FMcpToolAutoRegistrar. */
	void Register(FMcpToolDefinition* Tool);

	/** Find a tool by name. Returns nullptr if not found. */
	FMcpToolDefinition* FindTool(const FString& Name) const;

	/** Get all registered tool definitions (copy to prevent external mutation). */
	TArray<FMcpToolDefinition*> GetAllTools() const { return Tools; }

	/** Get all tool names. */
	TSet<FString> GetToolNames() const;

	/** Get category for a tool (default "utility"). */
	FString GetToolCategory(const FString& ToolName) const;

	/** Get category map (tool name -> category). */
	TMap<FString, FString> GetToolCategories() const;

	/** Get total number of registered tools. */
	int32 GetToolCount() const { return Tools.Num(); }

	/**
	 * Build the tools/list response filtered to enabled tools.
	 * Caches per-tool JSON objects on first call.
	 */
	TSharedPtr<FJsonObject> GetFilteredToolsResponse(const TSet<FString>& EnabledTools);

	/** Invalidate cached schemas (e.g. if tools are dynamically added at runtime). */
	void InvalidateCache();

private:
	FMcpToolRegistry() = default;

	TArray<FMcpToolDefinition*> Tools;
	TMap<FString, FMcpToolDefinition*> ToolsByName;

	// Cached per-tool JSON schemas (built lazily on first GetFilteredToolsResponse)
	TMap<FString, TSharedPtr<FJsonObject>> CachedToolSchemas;
	bool bCacheValid = false;
	mutable FCriticalSection CacheMutex;  // protects CachedToolSchemas + bCacheValid

	void EnsureCache();  // caller must hold CacheMutex
	TSharedPtr<FJsonObject> BuildToolJson(FMcpToolDefinition* Tool);
};

/**
 * Static auto-registrar. Place one per tool .cpp file via MCP_REGISTER_TOOL.
 */
struct FMcpToolAutoRegistrar
{
	FMcpToolAutoRegistrar(FMcpToolDefinition* Tool)
	{
		FMcpToolRegistry::Get().Register(Tool);
	}
};

/**
 * Auto-register a tool definition class.
 * Creates a static instance and registers it with FMcpToolRegistry.
 * Unity-build safe: variable names include the class name, ensuring uniqueness.
 */
#define MCP_REGISTER_TOOL(ToolClass) \
	static ToolClass MCP_TOOL_INSTANCE_##ToolClass; \
	static FMcpToolAutoRegistrar MCP_TOOL_REG_##ToolClass(&MCP_TOOL_INSTANCE_##ToolClass);

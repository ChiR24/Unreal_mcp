#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Loads and caches MCP tool schemas from a JSON file.
 * Serves the cached data for tools/list responses.
 */
class FMcpToolSchemaLoader
{
public:
	/** Load tool schemas from Resources/MCP/tool-schemas.json. Returns true on success. */
	bool LoadFromFile(const FString& PluginDir);

	/** Return the cached tools/list response (ready to wrap in JSON-RPC envelope). */
	TSharedPtr<FJsonObject> GetToolsListResponse() const;

	/** Check if a tool name exists in the loaded schemas. */
	bool HasTool(const FString& ToolName) const;

	/** Number of loaded tools. */
	int32 GetToolCount() const;

	/** Get category for a tool. Returns empty string if unknown. */
	FString GetToolCategory(const FString& ToolName) const;

	/** Get all tool names. */
	const TSet<FString>& GetToolNames() const { return ToolNames; }

	/** Get category map (tool name → category). */
	const TMap<FString, FString>& GetToolCategories() const { return ToolCategories; }

	/** Build a tools/list response filtered to only the enabled tools. */
	TSharedPtr<FJsonObject> GetFilteredToolsResponse(const TSet<FString>& EnabledTools) const;

private:
	TSharedPtr<FJsonObject> CachedToolsList;
	TSet<FString> ToolNames;
	TMap<FString, FString> ToolCategories;
	TArray<TSharedPtr<FJsonValue>> AllToolsArray;
};

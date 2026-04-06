#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpToolSchemaLoader;

DECLARE_DELEGATE(FOnToolsChanged);

/**
 * Manages MCP tool visibility at runtime.
 * Port of src/tools/dynamic-tool-manager.ts.
 */
class FMcpDynamicToolManager
{
public:
	/** Initialize from loaded schemas. bLoadAllTools=true enables all, false enables only core. */
	void Initialize(const FMcpToolSchemaLoader& SchemaLoader, bool bLoadAllTools = false);

	/** Check if a tool is enabled (tool AND category must be enabled). */
	bool IsToolEnabled(const FString& ToolName) const;

	/** Get set of all currently enabled tool names. */
	TSet<FString> GetEnabledToolNames() const;

	/** Dispatch a manage_tools action. Returns JSON result for the response. */
	TSharedPtr<FJsonObject> HandleAction(const FString& Action,
		const TSharedPtr<FJsonObject>& Args);

	/** Fired after any mutation that changes the enabled tool set. */
	FOnToolsChanged OnToolsChanged;

private:
	struct FToolState
	{
		FString Name;
		FString Category;
		bool bEnabled = true;
	};

	struct FCategoryState
	{
		FString Name;
		bool bEnabled = true;
		int32 ToolCount = 0;
		int32 EnabledCount = 0;
	};

	TMap<FString, FToolState> ToolStates;
	TMap<FString, FCategoryState> CategoryStates;

	/** Snapshot of initial enabled state from Initialize(), keyed by tool name. */
	TMap<FString, bool> InitialToolEnabled;
	TMap<FString, bool> InitialCategoryEnabled;

	// Actions
	TSharedPtr<FJsonObject> ListTools();
	TSharedPtr<FJsonObject> ListCategories();
	TSharedPtr<FJsonObject> EnableTools(const TArray<FString>& ToolNames);
	TSharedPtr<FJsonObject> DisableTools(const TArray<FString>& ToolNames);
	TSharedPtr<FJsonObject> EnableCategory(const FString& Category);
	TSharedPtr<FJsonObject> DisableCategory(const FString& Category);
	TSharedPtr<FJsonObject> GetStatus();
	TSharedPtr<FJsonObject> Reset();

	static bool IsProtectedTool(const FString& Name);
	static bool IsProtectedCategory(const FString& Name);
};

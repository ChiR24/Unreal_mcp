#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpToolRegistry;

DECLARE_DELEGATE(FOnToolsChanged);

/**
 * Manages MCP tool visibility at runtime.
 * Port of src/tools/dynamic-tool-manager.ts.
 */
class FMcpDynamicToolManager
{
public:
	/** Initialize from tool registry (self-describing C++ tool classes). */
	void Initialize(const FMcpToolRegistry& Registry, bool bLoadAllTools = false);

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

	/** Protects ToolStates, CategoryStates, InitialToolEnabled, InitialCategoryEnabled. */
	mutable FCriticalSection StateMutex;

	/** Lock-free impl — caller must hold StateMutex. */
	bool IsToolEnabled_NoLock(const FString& ToolName) const;

	// Actions
	TSharedPtr<FJsonObject> ListTools();
	TSharedPtr<FJsonObject> ListCategories();
	TSharedPtr<FJsonObject> EnableTools(const TArray<FString>& ToolNames, bool& bOutChanged);
	TSharedPtr<FJsonObject> DisableTools(const TArray<FString>& ToolNames, bool& bOutChanged);
	TSharedPtr<FJsonObject> EnableCategory(const FString& Category, bool& bOutChanged);
	TSharedPtr<FJsonObject> DisableCategory(const FString& Category, bool& bOutChanged);
	TSharedPtr<FJsonObject> GetStatus();
	TSharedPtr<FJsonObject> Reset(bool& bOutChanged);

	static bool IsProtectedTool(const FString& Name);
	static bool IsProtectedCategory(const FString& Name);
};

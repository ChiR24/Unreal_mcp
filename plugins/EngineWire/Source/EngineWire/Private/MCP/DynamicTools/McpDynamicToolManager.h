#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpToolRegistry;

/**
 * Manages MCP tool visibility at runtime.
 * Port of src/tools/dynamic/dynamic-tool-manager.ts.
 */
class FMcpDynamicToolManager
{
public:
	/** Initialize from tool registry (self-describing C++ tool classes). */
	void Initialize(const FMcpToolRegistry& Registry, bool bLoadAllTools = false);

	/** Check if a tool is enabled (tool AND category must be enabled). */
	bool IsToolEnabled(const FString& ToolName) const;

	/** Dispatch a manage_tools action. Returns JSON result for the response. */
	TSharedPtr<FJsonObject> HandleAction(const FString& Action,
		const TSharedPtr<FJsonObject>& Args);

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
	};

	TMap<FString, FToolState> ToolStates;
	TMap<FString, FCategoryState> CategoryStates;

	/** Snapshot of initial enabled state from Initialize(), keyed by tool name. */
	TMap<FString, bool> InitialToolEnabled;
	TMap<FString, bool> InitialCategoryEnabled;

	/**
	 * Protects ToolStates, CategoryStates, InitialToolEnabled,
	 * InitialCategoryEnabled and CatalogStateRevision.
	 */
	mutable FCriticalSection StateMutex;

	/**
	 * Process-local count of effective visibility mutations, reported by
	 * get_status. Runtime state, not the generated catalog's content revision.
	 */
	uint64 CatalogStateRevision = 0;

	/** Lock-free impl — caller must hold StateMutex. */
	bool IsToolEnabled_NoLock(const FString& ToolName) const;

	// Actions
	TSharedPtr<FJsonObject> ListTools();
	TSharedPtr<FJsonObject> ListCategories();
	/** Categories with their tool counts, counted on read. */
	TArray<TSharedPtr<FJsonValue>> DescribeCategories_NoLock() const;
	TSharedPtr<FJsonObject> EnableTools(const TArray<FString>& ToolNames, bool& bOutChanged);
	TSharedPtr<FJsonObject> DisableTools(const TArray<FString>& ToolNames, bool& bOutChanged);
	TSharedPtr<FJsonObject> EnableCategory(const FString& Category, bool& bOutChanged);
	TSharedPtr<FJsonObject> DisableCategory(const FString& Category, bool& bOutChanged);
	TSharedPtr<FJsonObject> GetStatus();
	TSharedPtr<FJsonObject> Reset(bool& bOutChanged);

	static bool IsProtectedTool(const FString& Name);
	static bool IsProtectedCategory(const FString& Name);
};

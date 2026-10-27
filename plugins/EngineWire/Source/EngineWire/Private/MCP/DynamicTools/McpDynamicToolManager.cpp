#include "MCP/DynamicTools/McpDynamicToolManager.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "MCP/Registry/McpToolRegistry.h"
#include "Misc/ScopeLock.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpToolManager, Log, All);

bool FMcpDynamicToolManager::IsProtectedTool(const FString& Name)
{
	return Name == TEXT("manage_tools") || Name == TEXT("inspect");
}

bool FMcpDynamicToolManager::IsProtectedCategory(const FString& Name)
{
	return Name == TEXT("core");
}

void FMcpDynamicToolManager::Initialize(const FMcpToolRegistry& Registry, bool bLoadAllTools)
{
	FScopeLock Lock(&StateMutex);
	ToolStates.Empty();
	CategoryStates.Empty();
	CatalogStateRevision = 0;

	for (const FString& ToolName : Registry.GetToolNames())
	{
		FString Category = Registry.GetToolCategory(ToolName);

		bool bEnabled = bLoadAllTools || (Category == TEXT("core"));

		FToolState& TS = ToolStates.Add(ToolName);
		TS.Name = ToolName;
		TS.Category = Category;
		TS.bEnabled = bEnabled;

		CategoryStates.FindOrAdd(Category).Name = Category;
	}

	InitialToolEnabled.Empty();
	InitialCategoryEnabled.Empty();
	for (const auto& Pair : ToolStates)
	{
		InitialToolEnabled.Add(Pair.Key, Pair.Value.bEnabled);
	}
	for (const auto& Pair : CategoryStates)
	{
		InitialCategoryEnabled.Add(Pair.Key, Pair.Value.bEnabled);
	}

	UE_LOG(LogMcpToolManager, Log, TEXT("Initialized from registry with %d tools across %d categories"),
		ToolStates.Num(), CategoryStates.Num());
}

bool FMcpDynamicToolManager::IsToolEnabled_NoLock(const FString& ToolName) const
{
	const FToolState* TS = ToolStates.Find(ToolName);
	if (!TS) return false;

	const FCategoryState* CS = CategoryStates.Find(TS->Category);
	return TS->bEnabled && (!CS || CS->bEnabled);
}

bool FMcpDynamicToolManager::IsToolEnabled(const FString& ToolName) const
{
	FScopeLock Lock(&StateMutex);
	return IsToolEnabled_NoLock(ToolName);
}
TSharedPtr<FJsonObject> FMcpDynamicToolManager::HandleAction(
	const FString& Action, const TSharedPtr<FJsonObject>& Args)
{
	if (Action == TEXT("list_tools"))
	{
		FScopeLock Lock(&StateMutex);
		return ListTools();
	}
	if (Action == TEXT("list_categories"))
	{
		FScopeLock Lock(&StateMutex);
		return ListCategories();
	}
	if (Action == TEXT("get_status"))
	{
		FScopeLock Lock(&StateMutex);
		return GetStatus();
	}

	if (Action != TEXT("reset") && !Args.IsValid())
	{
		auto Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), FString::Printf(TEXT("Action '%s' requires arguments"), *Action));
		return Err;
	}

	// The revision bump shares the mutation's lock, so a status read never sees
	// new state with a stale revision.
	FScopeLock Lock(&StateMutex);
	bool bChanged = false;
	TSharedPtr<FJsonObject> Result;
	if (Action == TEXT("reset")) Result = Reset(bChanged);
	else if (Action == TEXT("enable_tools")) Result = EnableTools(McpHandlerUtils::GetStringArrayField(Args, TEXT("tools")), bChanged);
	else if (Action == TEXT("disable_tools")) Result = DisableTools(McpHandlerUtils::GetStringArrayField(Args, TEXT("tools")), bChanged);
	else if (Action == TEXT("enable_category")) Result = EnableCategory(GetJsonStringField(Args, TEXT("category")), bChanged);
	else if (Action == TEXT("disable_category")) Result = DisableCategory(GetJsonStringField(Args, TEXT("category")), bChanged);
	if (Result.IsValid())
	{
		if (bChanged) ++CatalogStateRevision;
		return Result;
	}

	auto Err = MakeShared<FJsonObject>();
	Err->SetBoolField(TEXT("success"), false);
	Err->SetStringField(TEXT("error"),
		FString::Printf(TEXT("Unknown action: %s"), *Action));
	return Err;
}

#include "MCP/McpDynamicToolManager.h"
#include "MCP/McpToolRegistry.h"
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

	for (const FString& ToolName : Registry.GetToolNames())
	{
		FString Category = Registry.GetToolCategory(ToolName);

		bool bEnabled = bLoadAllTools || (Category == TEXT("core"));

		FToolState& TS = ToolStates.Add(ToolName);
		TS.Name = ToolName;
		TS.Category = Category;
		TS.bEnabled = bEnabled;

		FCategoryState& CS = CategoryStates.FindOrAdd(Category);
		if (CS.Name.IsEmpty())
		{
			CS.Name = Category;
			CS.bEnabled = true;
			CS.ToolCount = 0;
			CS.EnabledCount = 0;
		}
		CS.ToolCount++;
		if (bEnabled) CS.EnabledCount++;
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

TSet<FString> FMcpDynamicToolManager::GetEnabledToolNames() const
{
	FScopeLock Lock(&StateMutex);
	TSet<FString> Result;
	for (const auto& Pair : ToolStates)
	{
		if (IsToolEnabled_NoLock(Pair.Key))
		{
			Result.Add(Pair.Key);
		}
	}
	return Result;
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

	bool bChanged = false;
	TSharedPtr<FJsonObject> Result;

	if (Action != TEXT("reset") && !Args.IsValid())
	{
		auto Err = MakeShared<FJsonObject>();
		Err->SetBoolField(TEXT("success"), false);
		Err->SetStringField(TEXT("error"), FString::Printf(TEXT("Action '%s' requires arguments"), *Action));
		return Err;
	}

	if (Action == TEXT("reset"))
	{
		{
			FScopeLock Lock(&StateMutex);
			Result = Reset(bChanged);
		}
		if (bChanged) OnToolsChanged.ExecuteIfBound();
		return Result;
	}

	if (Action == TEXT("enable_tools"))
	{
		TArray<FString> Names;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Args->TryGetArrayField(TEXT("tools"), Arr) && Arr)
		{
			for (const auto& V : *Arr)
			{
				FString S;
				if (V->TryGetString(S)) Names.Add(S);
			}
		}
		{
			FScopeLock Lock(&StateMutex);
			Result = EnableTools(Names, bChanged);
		}
		if (bChanged) OnToolsChanged.ExecuteIfBound();
		return Result;
	}

	if (Action == TEXT("disable_tools"))
	{
		TArray<FString> Names;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Args->TryGetArrayField(TEXT("tools"), Arr) && Arr)
		{
			for (const auto& V : *Arr)
			{
				FString S;
				if (V->TryGetString(S)) Names.Add(S);
			}
		}
		{
			FScopeLock Lock(&StateMutex);
			Result = DisableTools(Names, bChanged);
		}
		if (bChanged) OnToolsChanged.ExecuteIfBound();
		return Result;
	}

	if (Action == TEXT("enable_category"))
	{
		FString Cat;
		Args->TryGetStringField(TEXT("category"), Cat);
		{
			FScopeLock Lock(&StateMutex);
			Result = EnableCategory(Cat, bChanged);
		}
		if (bChanged) OnToolsChanged.ExecuteIfBound();
		return Result;
	}

	if (Action == TEXT("disable_category"))
	{
		FString Cat;
		Args->TryGetStringField(TEXT("category"), Cat);
		{
			FScopeLock Lock(&StateMutex);
			Result = DisableCategory(Cat, bChanged);
		}
		if (bChanged) OnToolsChanged.ExecuteIfBound();
		return Result;
	}

	auto Err = MakeShared<FJsonObject>();
	Err->SetBoolField(TEXT("success"), false);
	Err->SetStringField(TEXT("error"),
		FString::Printf(TEXT("Unknown action: %s"), *Action));
	return Err;
}

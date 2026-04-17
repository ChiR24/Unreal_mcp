#include "MCP/McpDynamicToolManager.h"
#include "MCP/McpToolRegistry.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpToolManager, Log, All);

// ─── Protected lists ────────────────────────────────────────────────────────

bool FMcpDynamicToolManager::IsProtectedTool(const FString& Name)
{
	return Name == TEXT("manage_tools") || Name == TEXT("inspect");
}

bool FMcpDynamicToolManager::IsProtectedCategory(const FString& Name)
{
	return Name == TEXT("core");
}

// ─── Initialize ─────────────────────────────────────────────────────────────

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
			CS.bEnabled = bEnabled;
			CS.ToolCount = 0;
			CS.EnabledCount = 0;
		}
		CS.ToolCount++;
		if (bEnabled) CS.EnabledCount++;
	}

	// Snapshot initial state for Reset()
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

// ─── Query ──────────────────────────────────────────────────────────────────

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

// ─── Action dispatch ────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMcpDynamicToolManager::HandleAction(
	const FString& Action, const TSharedPtr<FJsonObject>& Args)
{
	// Read-only actions — lock, dispatch, return
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

	// Mutating actions — parse args outside lock, mutate under lock, fire delegate after unlock
	bool bChanged = false;
	TSharedPtr<FJsonObject> Result;

	// reset doesn't need args; everything below does
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

	// Unknown action — no lock needed
	auto Err = MakeShared<FJsonObject>();
	Err->SetBoolField(TEXT("success"), false);
	Err->SetStringField(TEXT("error"),
		FString::Printf(TEXT("Unknown action: %s"), *Action));
	return Err;
}

// ─── List Tools ─────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMcpDynamicToolManager::ListTools()
{
	TArray<TSharedPtr<FJsonValue>> ToolsArr;
	for (const auto& Pair : ToolStates)
	{
		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Pair.Value.Name);
		Obj->SetBoolField(TEXT("enabled"), IsToolEnabled_NoLock(Pair.Key));
		Obj->SetStringField(TEXT("category"), Pair.Value.Category);
		ToolsArr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("tools"), ToolsArr);
	Result->SetNumberField(TEXT("totalTools"), ToolStates.Num());
	return Result;
}

// ─── List Categories ────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMcpDynamicToolManager::ListCategories()
{
	TArray<TSharedPtr<FJsonValue>> CatsArr;
	for (const auto& Pair : CategoryStates)
	{
		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Pair.Value.Name);
		Obj->SetBoolField(TEXT("enabled"), Pair.Value.bEnabled);
		Obj->SetNumberField(TEXT("toolCount"), Pair.Value.ToolCount);
		Obj->SetNumberField(TEXT("enabledCount"), Pair.Value.EnabledCount);
		CatsArr.Add(MakeShared<FJsonValueObject>(Obj));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("categories"), CatsArr);
	return Result;
}

// ─── Enable Tools ───────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMcpDynamicToolManager::EnableTools(const TArray<FString>& ToolNames, bool& bOutChanged)
{
	TArray<TSharedPtr<FJsonValue>> Enabled;
	TArray<TSharedPtr<FJsonValue>> NotFound;
	bool bAnyActualChange = false;

	for (const FString& Name : ToolNames)
	{
		FToolState* TS = ToolStates.Find(Name);
		if (TS)
		{
			if (!TS->bEnabled)
			{
				TS->bEnabled = true;
				FCategoryState* CS = CategoryStates.Find(TS->Category);
				if (CS) CS->EnabledCount++;
				bAnyActualChange = true;
			}
			Enabled.Add(MakeShared<FJsonValueString>(Name));
		}
		else
		{
			NotFound.Add(MakeShared<FJsonValueString>(Name));
		}
	}

	bOutChanged = bAnyActualChange;

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("enabled"), Enabled);
	Result->SetArrayField(TEXT("notFound"), NotFound);
	return Result;
}

// ─── Disable Tools ──────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMcpDynamicToolManager::DisableTools(const TArray<FString>& ToolNames, bool& bOutChanged)
{
	TArray<TSharedPtr<FJsonValue>> Disabled;
	TArray<TSharedPtr<FJsonValue>> NotFound;
	TArray<TSharedPtr<FJsonValue>> Protected;
	bool bAnyActualChange = false;

	for (const FString& Name : ToolNames)
	{
		if (IsProtectedTool(Name))
		{
			Protected.Add(MakeShared<FJsonValueString>(Name));
			continue;
		}

		FToolState* TS = ToolStates.Find(Name);
		if (TS)
		{
			if (TS->bEnabled)
			{
				TS->bEnabled = false;
				FCategoryState* CS = CategoryStates.Find(TS->Category);
				if (CS && CS->EnabledCount > 0) CS->EnabledCount--;
				bAnyActualChange = true;
			}
			Disabled.Add(MakeShared<FJsonValueString>(Name));
		}
		else
		{
			NotFound.Add(MakeShared<FJsonValueString>(Name));
		}
	}

	bOutChanged = bAnyActualChange;

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("disabled"), Disabled);
	Result->SetArrayField(TEXT("notFound"), NotFound);
	Result->SetArrayField(TEXT("protected"), Protected);
	return Result;
}

// ─── Enable Category ────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMcpDynamicToolManager::EnableCategory(const FString& Category, bool& bOutChanged)
{
	TArray<TSharedPtr<FJsonValue>> Enabled;

	bool bAnyCategoryToggled = false;

	if (Category == TEXT("all"))
	{
		for (auto& Pair : CategoryStates)
		{
			if (IsProtectedCategory(Pair.Key))
			{
				const bool* Initial = InitialCategoryEnabled.Find(Pair.Key);
				bool bTarget = Initial ? *Initial : true;
				if (!Pair.Value.bEnabled && bTarget) bAnyCategoryToggled = true;
				Pair.Value.bEnabled = bTarget;
			}
			else
			{
				if (!Pair.Value.bEnabled) bAnyCategoryToggled = true;
				Pair.Value.bEnabled = true;
			}
		}
		for (auto& Pair : ToolStates)
		{
			FCategoryState* CS = CategoryStates.Find(Pair.Value.Category);
			bool bCategoryEnabled = CS ? CS->bEnabled : true;

			if (IsProtectedTool(Pair.Key))
			{
				const bool* InitialTool = InitialToolEnabled.Find(Pair.Key);
				bool bToolTarget = (InitialTool ? *InitialTool : true) && bCategoryEnabled;
				if (!Pair.Value.bEnabled && bToolTarget)
				{
					Pair.Value.bEnabled = true;
					Enabled.Add(MakeShared<FJsonValueString>(Pair.Key));
				}
			}
			else if (bCategoryEnabled && !Pair.Value.bEnabled)
			{
				Pair.Value.bEnabled = true;
				Enabled.Add(MakeShared<FJsonValueString>(Pair.Key));
			}
		}
		for (auto& CatPair : CategoryStates)
		{
			CatPair.Value.EnabledCount = 0;
			for (const auto& ToolPair : ToolStates)
			{
				if (ToolPair.Value.Category == CatPair.Key && ToolPair.Value.bEnabled)
				{
					CatPair.Value.EnabledCount++;
				}
			}
		}
	}
	else
	{
		FCategoryState* CS = CategoryStates.Find(Category);
		if (!CS)
		{
			auto Err = MakeShared<FJsonObject>();
			Err->SetBoolField(TEXT("success"), false);
			Err->SetStringField(TEXT("error"),
				FString::Printf(TEXT("Category '%s' not found"), *Category));
			return Err;
		}

		if (!CS->bEnabled) bAnyCategoryToggled = true;
		CS->bEnabled = true;
		for (auto& Pair : ToolStates)
		{
			if (Pair.Value.Category == Category && !Pair.Value.bEnabled)
			{
				Pair.Value.bEnabled = true;
				Enabled.Add(MakeShared<FJsonValueString>(Pair.Key));
			}
		}
		CS->EnabledCount = CS->ToolCount;
	}

	bOutChanged = (Enabled.Num() > 0) || bAnyCategoryToggled;

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("category"), Category);
	Result->SetArrayField(TEXT("enabled"), Enabled);
	return Result;
}

// ─── Disable Category ───────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMcpDynamicToolManager::DisableCategory(const FString& Category, bool& bOutChanged)
{
	TArray<TSharedPtr<FJsonValue>> Disabled;
	TArray<TSharedPtr<FJsonValue>> Protected;
	bool bAnyCategoryToggled = false;

	if (Category == TEXT("all"))
	{
		for (auto& CatPair : CategoryStates)
		{
			if (IsProtectedCategory(CatPair.Key))
			{
				CatPair.Value.bEnabled = true;
			}
			else
			{
				if (CatPair.Value.bEnabled) bAnyCategoryToggled = true;
				CatPair.Value.bEnabled = false;
				CatPair.Value.EnabledCount = 0;
			}
		}
		for (auto& Pair : ToolStates)
		{
			if (IsProtectedTool(Pair.Key) || IsProtectedCategory(Pair.Value.Category))
			{
				Protected.Add(MakeShared<FJsonValueString>(Pair.Key));
			}
			else if (Pair.Value.bEnabled)
			{
				Pair.Value.bEnabled = false;
				Disabled.Add(MakeShared<FJsonValueString>(Pair.Key));
			}
		}
		// Recalculate EnabledCount for protected categories
		for (auto& CatPair : CategoryStates)
		{
			if (IsProtectedCategory(CatPair.Key))
			{
				CatPair.Value.EnabledCount = 0;
				for (const auto& ToolPair : ToolStates)
				{
					if (ToolPair.Value.Category == CatPair.Key && ToolPair.Value.bEnabled)
					{
						CatPair.Value.EnabledCount++;
					}
				}
			}
		}
	}
	else
	{
		FCategoryState* CS = CategoryStates.Find(Category);
		if (!CS)
		{
			auto Err = MakeShared<FJsonObject>();
			Err->SetBoolField(TEXT("success"), false);
			Err->SetStringField(TEXT("error"),
				FString::Printf(TEXT("Category '%s' not found"), *Category));
			return Err;
		}

		if (IsProtectedCategory(Category))
		{
			auto Err = MakeShared<FJsonObject>();
			Err->SetBoolField(TEXT("success"), false);
			Err->SetStringField(TEXT("error"),
				FString::Printf(TEXT("Category '%s' is protected and cannot be disabled"), *Category));
			return Err;
		}

		if (CS->bEnabled) bAnyCategoryToggled = true;
		CS->bEnabled = false;

		for (auto& Pair : ToolStates)
		{
			if (Pair.Value.Category == Category)
			{
				if (IsProtectedTool(Pair.Key))
				{
					Protected.Add(MakeShared<FJsonValueString>(Pair.Key));
				}
				else if (Pair.Value.bEnabled)
				{
					Pair.Value.bEnabled = false;
					Disabled.Add(MakeShared<FJsonValueString>(Pair.Key));
				}
			}
		}

		// Recount
		CS->EnabledCount = 0;
		for (const auto& Pair : ToolStates)
		{
			if (Pair.Value.Category == Category && Pair.Value.bEnabled)
			{
				CS->EnabledCount++;
			}
		}
	}

	bOutChanged = (Disabled.Num() > 0) || bAnyCategoryToggled;

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("category"), Category);
	Result->SetArrayField(TEXT("disabled"), Disabled);
	Result->SetArrayField(TEXT("protected"), Protected);
	return Result;
}

// ─── Get Status ─────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMcpDynamicToolManager::GetStatus()
{
	int32 EnabledCount = 0;
	for (const auto& Pair : ToolStates)
	{
		if (IsToolEnabled_NoLock(Pair.Key)) EnabledCount++;
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("totalTools"), ToolStates.Num());
	Result->SetNumberField(TEXT("enabledTools"), EnabledCount);
	Result->SetNumberField(TEXT("disabledTools"), ToolStates.Num() - EnabledCount);

	// Include categories
	TArray<TSharedPtr<FJsonValue>> CatsArr;
	for (const auto& Pair : CategoryStates)
	{
		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Pair.Value.Name);
		Obj->SetBoolField(TEXT("enabled"), Pair.Value.bEnabled);
		Obj->SetNumberField(TEXT("toolCount"), Pair.Value.ToolCount);
		Obj->SetNumberField(TEXT("enabledCount"), Pair.Value.EnabledCount);
		CatsArr.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Result->SetArrayField(TEXT("categories"), CatsArr);
	return Result;
}

// ─── Reset ──────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FMcpDynamicToolManager::Reset(bool& bOutChanged)
{
	int32 Changed = 0;

	// Restore tool states to initial
	for (auto& Pair : ToolStates)
	{
		const bool* Initial = InitialToolEnabled.Find(Pair.Key);
		bool bTarget = Initial ? *Initial : true;
		if (Pair.Value.bEnabled != bTarget)
		{
			Pair.Value.bEnabled = bTarget;
			Changed++;
		}
	}

	// Restore category states and recount
	for (auto& Pair : CategoryStates)
	{
		const bool* Initial = InitialCategoryEnabled.Find(Pair.Key);
		bool bTarget = Initial ? *Initial : true;
		if (Pair.Value.bEnabled != bTarget)
		{
			Pair.Value.bEnabled = bTarget;
			Changed++;
		}
		Pair.Value.EnabledCount = 0;
	}
	for (const auto& Pair : ToolStates)
	{
		FCategoryState* CS = CategoryStates.Find(Pair.Value.Category);
		if (CS && Pair.Value.bEnabled) CS->EnabledCount++;
	}

	bOutChanged = (Changed > 0);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("changed"), Changed);
	Result->SetStringField(TEXT("message"),
		FString::Printf(TEXT("Reset to initial state. %d tools changed."), Changed));
	return Result;
}

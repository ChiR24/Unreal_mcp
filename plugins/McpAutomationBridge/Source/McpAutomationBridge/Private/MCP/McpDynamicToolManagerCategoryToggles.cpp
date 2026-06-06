#include "MCP/McpDynamicToolManager.h"

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
				const bool bTarget = Initial ? *Initial : true;
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
			const bool bCategoryEnabled = CS ? CS->bEnabled : true;

			if (IsProtectedTool(Pair.Key))
			{
				const bool* InitialTool = InitialToolEnabled.Find(Pair.Key);
				const bool bToolTarget = (InitialTool ? *InitialTool : true) && bCategoryEnabled;
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

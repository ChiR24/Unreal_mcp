#include "MCP/McpDynamicToolManager.h"

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

TSharedPtr<FJsonObject> FMcpDynamicToolManager::Reset(bool& bOutChanged)
{
	int32 Changed = 0;
	for (auto& Pair : ToolStates)
	{
		const bool* Initial = InitialToolEnabled.Find(Pair.Key);
		const bool bTarget = Initial ? *Initial : true;
		if (Pair.Value.bEnabled != bTarget)
		{
			Pair.Value.bEnabled = bTarget;
			Changed++;
		}
	}

	for (auto& Pair : CategoryStates)
	{
		const bool* Initial = InitialCategoryEnabled.Find(Pair.Key);
		const bool bTarget = Initial ? *Initial : true;
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

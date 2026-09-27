#include "MCP/DynamicTools/McpDynamicToolManager.h"
#include "MCP/Generated/McpGeneratedCapabilityShards.h"

TSharedPtr<FJsonObject> FMcpDynamicToolManager::ListTools()
{
	TArray<TSharedPtr<FJsonValue>> ToolsArr;
	int32 EnabledCount = 0;
	for (const auto& Pair : ToolStates)
	{
		const bool bEnabled = IsToolEnabled_NoLock(Pair.Key);
		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Pair.Value.Name);
		Obj->SetBoolField(TEXT("enabled"), bEnabled);
		Obj->SetStringField(TEXT("category"), Pair.Value.Category);
		ToolsArr.Add(MakeShared<FJsonValueObject>(Obj));
		if (bEnabled) ++EnabledCount;
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("tools"), ToolsArr);
	Result->SetNumberField(TEXT("totalTools"), ToolStates.Num());
	Result->SetNumberField(TEXT("enabledCount"), EnabledCount);
	Result->SetNumberField(TEXT("disabledCount"), ToolStates.Num() - EnabledCount);
	return Result;
}

TArray<TSharedPtr<FJsonValue>> FMcpDynamicToolManager::DescribeCategories_NoLock() const
{
	TMap<FString, TPair<int32, int32>> Counts; // category -> (tools, enabled tools)
	for (const auto& Pair : ToolStates)
	{
		TPair<int32, int32>& Count = Counts.FindOrAdd(Pair.Value.Category);
		++Count.Key;
		Count.Value += Pair.Value.bEnabled ? 1 : 0;
	}
	TArray<TSharedPtr<FJsonValue>> CatsArr;
	for (const auto& Pair : CategoryStates)
	{
		const TPair<int32, int32> Count = Counts.FindRef(Pair.Key);
		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Pair.Value.Name);
		Obj->SetBoolField(TEXT("enabled"), Pair.Value.bEnabled);
		Obj->SetNumberField(TEXT("toolCount"), Count.Key);
		Obj->SetNumberField(TEXT("enabledCount"), Count.Value);
		CatsArr.Add(MakeShared<FJsonValueObject>(Obj));
	}
	return CatsArr;
}

TSharedPtr<FJsonObject> FMcpDynamicToolManager::ListCategories()
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetArrayField(TEXT("categories"), DescribeCategories_NoLock());
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
	Result->SetStringField(TEXT("catalogRevision"), McpGeneratedCapabilityShards::CatalogRevision());
	Result->SetNumberField(TEXT("catalogStateRevision"), static_cast<double>(CatalogStateRevision));

	Result->SetArrayField(TEXT("categories"), DescribeCategories_NoLock());
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
	}

	bOutChanged = (Changed > 0);

	auto Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("changed"), Changed);
	Result->SetStringField(TEXT("message"),
		FString::Printf(TEXT("Reset to initial state. %d tools changed."), Changed));
	return Result;
}

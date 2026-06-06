#include "MCP/McpDynamicToolManager.h"

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
			FCategoryState* CS = CategoryStates.Find(TS->Category);
			if (CS && !CS->bEnabled)
			{
				CS->bEnabled = true;
				bAnyActualChange = true;
			}
			if (!TS->bEnabled)
			{
				TS->bEnabled = true;
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

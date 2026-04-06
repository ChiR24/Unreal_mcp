#include "MCP/McpToolSchemaLoader.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogMcpSchema, Log, All);

bool FMcpToolSchemaLoader::LoadFromFile(const FString& PluginDir)
{
	const FString FilePath = PluginDir / TEXT("Resources/MCP/tool-schemas.json");
	FString JsonString;

	if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
	{
		UE_LOG(LogMcpSchema, Error, TEXT("Failed to load tool schemas from %s"), *FilePath);
		return false;
	}

	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	TSharedPtr<FJsonObject> Root;

	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogMcpSchema, Error, TEXT("Failed to parse tool-schemas.json"));
		return false;
	}

	// Validate structure
	const TArray<TSharedPtr<FJsonValue>>* ToolsArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("tools"), ToolsArray) || !ToolsArray)
	{
		UE_LOG(LogMcpSchema, Error, TEXT("tool-schemas.json missing 'tools' array"));
		return false;
	}

	// Build tool name and category indices
	ToolNames.Empty();
	ToolCategories.Empty();
	AllToolsArray = *ToolsArray;

	for (const auto& ToolVal : AllToolsArray)
	{
		const TSharedPtr<FJsonObject>* ToolObj = nullptr;
		if (ToolVal->TryGetObject(ToolObj) && ToolObj && (*ToolObj)->HasField(TEXT("name")))
		{
			FString Name = (*ToolObj)->GetStringField(TEXT("name"));
			ToolNames.Add(Name);

			FString Category;
			if ((*ToolObj)->TryGetStringField(TEXT("category"), Category))
			{
				ToolCategories.Add(Name, Category);
			}
			else
			{
				ToolCategories.Add(Name, TEXT("utility"));
			}
		}
	}

	CachedToolsList = Root;

	UE_LOG(LogMcpSchema, Log, TEXT("Loaded %d tool schemas from %s"), ToolNames.Num(), *FilePath);
	return true;
}

TSharedPtr<FJsonObject> FMcpToolSchemaLoader::GetToolsListResponse() const
{
	return CachedToolsList;
}

bool FMcpToolSchemaLoader::HasTool(const FString& ToolName) const
{
	return ToolNames.Contains(ToolName);
}

int32 FMcpToolSchemaLoader::GetToolCount() const
{
	return ToolNames.Num();
}

FString FMcpToolSchemaLoader::GetToolCategory(const FString& ToolName) const
{
	const FString* Cat = ToolCategories.Find(ToolName);
	return Cat ? *Cat : FString();
}

TSharedPtr<FJsonObject> FMcpToolSchemaLoader::GetFilteredToolsResponse(
	const TSet<FString>& EnabledTools) const
{
	TArray<TSharedPtr<FJsonValue>> Filtered;
	for (const auto& ToolVal : AllToolsArray)
	{
		const TSharedPtr<FJsonObject>* ToolObj = nullptr;
		if (ToolVal->TryGetObject(ToolObj) && ToolObj)
		{
			FString Name;
			if ((*ToolObj)->TryGetStringField(TEXT("name"), Name) && EnabledTools.Contains(Name))
			{
				Filtered.Add(ToolVal);
			}
		}
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("tools"), Filtered);
	return Result;
}

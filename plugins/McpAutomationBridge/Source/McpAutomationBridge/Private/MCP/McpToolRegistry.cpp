// McpToolRegistry.cpp — Singleton registry for self-describing MCP tool definitions

#include "McpVersionCompatibility.h"
#include "MCP/McpToolRegistry.h"
#include "MCP/McpToolDefinition.h"

namespace
{
bool IsCanonicalMcpToolName(const FString& Name)
{
	static const TSet<FString> CanonicalToolNames = {
		TEXT("manage_tools"),
		TEXT("manage_asset"),
		TEXT("manage_blueprint"),
		TEXT("control_actor"),
		TEXT("control_editor"),
		TEXT("manage_level"),
		TEXT("build_environment"),
		TEXT("animation_physics"),
		TEXT("system_control"),
		TEXT("manage_sequence"),
		TEXT("inspect"),
		TEXT("manage_audio"),
		TEXT("manage_geometry"),
		TEXT("manage_effect"),
		TEXT("manage_gas"),
		TEXT("manage_character"),
		TEXT("manage_combat"),
		TEXT("manage_ai"),
		TEXT("manage_inventory"),
		TEXT("manage_interaction"),
		TEXT("manage_networking"),
		TEXT("manage_level_structure"),
		TEXT("manage_pcg")
	};

	return CanonicalToolNames.Contains(Name);
}
}

FMcpToolRegistry& FMcpToolRegistry::Get()
{
	static FMcpToolRegistry Instance;
	return Instance;
}

void FMcpToolRegistry::Register(FMcpToolDefinition* Tool)
{
	if (!Tool)
	{
		return;
	}

	FScopeLock Lock(&CacheMutex);
	const FString Name = Tool->GetName();
	if (!IsCanonicalMcpToolName(Name))
	{
		return;
	}

	if (ToolsByName.Contains(Name))
	{
		return; // Already registered (possible with unity builds reloading)
	}

	Tools.Add(Tool);
	ToolsByName.Add(Name, Tool);
	bCacheValid = false;
}

FMcpToolDefinition* FMcpToolRegistry::FindTool(const FString& Name) const
{
	if (const auto* Found = ToolsByName.Find(Name))
	{
		return *Found;
	}
	return nullptr;
}

TSet<FString> FMcpToolRegistry::GetToolNames() const
{
	TSet<FString> Names;
	Names.Reserve(Tools.Num());
	for (const FMcpToolDefinition* Tool : Tools)
	{
		Names.Add(Tool->GetName());
	}
	return Names;
}

FString FMcpToolRegistry::GetToolCategory(const FString& ToolName) const
{
	if (const FMcpToolDefinition* Tool = FindTool(ToolName))
	{
		return Tool->GetCategory();
	}
	return TEXT("utility");
}

TMap<FString, FString> FMcpToolRegistry::GetToolCategories() const
{
	TMap<FString, FString> Categories;
	Categories.Reserve(Tools.Num());
	for (const FMcpToolDefinition* Tool : Tools)
	{
		Categories.Add(Tool->GetName(), Tool->GetCategory());
	}
	return Categories;
}

void FMcpToolRegistry::EnsureCache()
{
	if (bCacheValid)
	{
		return;
	}

	CachedToolSchemas.Empty();
	CachedToolSchemas.Reserve(Tools.Num());
	for (FMcpToolDefinition* Tool : Tools)
	{
		CachedToolSchemas.Add(Tool->GetName(), BuildToolJson(Tool));
	}
	bCacheValid = true;
}

TSharedPtr<FJsonObject> FMcpToolRegistry::BuildToolJson(FMcpToolDefinition* Tool)
{
	auto ToolObj = MakeShared<FJsonObject>();
	ToolObj->SetStringField(TEXT("name"), Tool->GetName());
	ToolObj->SetStringField(TEXT("description"), Tool->GetDescription());
	ToolObj->SetStringField(TEXT("category"), Tool->GetCategory());

	TSharedPtr<FJsonObject> InputSchema = Tool->BuildInputSchema();
	if (InputSchema.IsValid())
	{
		ToolObj->SetObjectField(TEXT("inputSchema"), InputSchema);
	}

	return ToolObj;
}

void FMcpToolRegistry::InvalidateCache()
{
	FScopeLock Lock(&CacheMutex);
	bCacheValid = false;
}
TSharedPtr<FJsonObject> FMcpToolRegistry::GetFilteredToolsResponse(
	const TSet<FString>& EnabledTools)
{
	FScopeLock Lock(&CacheMutex);
	EnsureCache();

	TArray<TSharedPtr<FJsonValue>> FilteredTools;
	FilteredTools.Reserve(EnabledTools.Num());

	for (const FMcpToolDefinition* Tool : Tools)
	{
		if (EnabledTools.Contains(Tool->GetName()))
		{
			if (const auto* Cached = CachedToolSchemas.Find(Tool->GetName()))
			{
				FilteredTools.Add(MakeShared<FJsonValueObject>(*Cached));
			}
		}
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("tools"), FilteredTools);
	return Result;
}

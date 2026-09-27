// McpToolRegistry.cpp — Singleton registry of the canonical parent tools

#include "MCP/Registry/McpToolRegistry.h"

#include "MCP/Registry/McpToolDefinition.h"

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

	FScopeLock Lock(&Mutex);
	const FString& Name = Tool->GetName();
	if (ToolsByName.Contains(Name))
	{
		return; // Already registered (possible with unity builds reloading)
	}

	Tools.Add(Tool);
	ToolsByName.Add(Name, Tool);
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

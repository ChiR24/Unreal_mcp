// McpToolRegistry.h — Singleton registry of the canonical parent tools

#pragma once

#include "CoreMinimal.h"

class FMcpToolDefinition;

/**
 * The canonical parent tools, registered at static-init time by the generated
 * McpGeneratedParentRegistry.cpp. Register() and the readers share one lock.
 */
class FMcpToolRegistry
{
public:
	static FMcpToolRegistry& Get();

	/** Register a tool definition; a second registration of a name is ignored. */
	void Register(FMcpToolDefinition* Tool);

	/** Find a tool by name. Returns nullptr if not found. */
	FMcpToolDefinition* FindTool(const FString& Name) const;

	TSet<FString> GetToolNames() const;

	/** Category for a tool (default "utility"). */
	FString GetToolCategory(const FString& ToolName) const;

	int32 GetToolCount() const { return Tools.Num(); }

private:
	FMcpToolRegistry() = default;

	TArray<FMcpToolDefinition*> Tools;
	TMap<FString, FMcpToolDefinition*> ToolsByName;
	mutable FCriticalSection Mutex;
};

// McpToolDefinition.h — one canonical parent tool's identity

#pragma once

#include "CoreMinimal.h"

/**
 * Name, description and category of one canonical parent tool. Pure data:
 * McpGeneratedParentRegistry.cpp registers one per parent, and every tool
 * dispatches on its own name.
 */
class FMcpToolDefinition
{
public:
	FMcpToolDefinition(const TCHAR* InName, const TCHAR* InDescription, const TCHAR* InCategory)
		: Name(InName), Description(InDescription), Category(InCategory)
	{
	}

	const FString& GetName() const { return Name; }
	const FString& GetDescription() const { return Description; }
	const FString& GetCategory() const { return Category; }

private:
	FString Name;
	FString Description;
	FString Category;
};

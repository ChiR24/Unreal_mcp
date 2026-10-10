// McpPromptCatalog.h
// The six workflow prompts served by prompts/list and prompts/get. Mirrors
// src/server/mcp-primitives/prompts.ts; nothing here executes or stores state.
#pragma once

#include "CoreMinimal.h"

struct FMcpPromptArgumentSpec
{
	FString Name;
	bool bRequired = false;
	FString Description;
	// Closed value set, offered by completion/complete; empty for free text.
	TArray<FString> Allowed;
};

struct FMcpPromptStep
{
	FString Summary;
	FString CapabilityId;
	FString ParentTool;
	FString Action;
	FString ResourceUri;
	FString Safety;
};

struct FMcpWorkflowPrompt
{
	FString Id;
	FString Title;
	FString Description;
	TArray<FMcpPromptArgumentSpec> Arguments;
	TArray<FMcpPromptStep> Steps;
};

const TArray<FMcpWorkflowPrompt>& McpWorkflowPrompts();

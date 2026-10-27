// McpPromptCatalog.h
// The prompts served by prompts/list and prompts/get: six step-by-step workflows and the per-domain playbooks
// (McpPromptPlaybooks.cpp), each with the traps of its domain. Nothing here executes or stores state.
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
	// What goes wrong in this domain and how to see it, rendered before the steps.
	TArray<FString> Traps;
};

const TArray<FMcpWorkflowPrompt>& McpWorkflowPrompts();

// The per-domain playbooks, appended to the workflows by McpWorkflowPrompts().
TArray<FMcpWorkflowPrompt> McpPlaybookPrompts();

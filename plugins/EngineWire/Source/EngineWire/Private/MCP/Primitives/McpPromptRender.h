// McpPromptRender.h
// prompts/list entries and the prompts/get render (mirrors prompts.ts).
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

struct FMcpPromptRenderResult
{
	bool bOk = false;
	FString ErrorMessage;
	FString Body;
	FString Description;
};

TArray<TSharedPtr<FJsonValue>> McpBuildPromptListEntries();

FMcpPromptRenderResult McpRenderWorkflowPrompt(const FString& Name, const TMap<FString, FString>& Args);

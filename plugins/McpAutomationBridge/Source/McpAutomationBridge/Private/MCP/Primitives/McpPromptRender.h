// McpPromptRender.h
// Task 38 lane B: the native prompts/list metadata and prompts/get render +
// argument validation. Mirrors the TypeScript prompt-catalog.ts getPrompt and
// prompt-errors.ts so the native transport serves the SAME rendered body and the
// SAME typed argument refusals (secret / unknown / missing / invalid / too large)
// as the TS surface, instead of a static stub. Pure logic over the canonical
// McpPromptCatalog data: it executes nothing, scans no editor, and never
// interpolates a secret. Wiring into prompts/* lives in the transport.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

struct FMcpPromptRenderResult
{
	bool bOk = false;
	FString ErrorCode;
	FString ErrorMessage;
	FString Body;
	FString Description;
};

TArray<TSharedPtr<FJsonValue>> McpBuildPromptListEntries();

FMcpPromptRenderResult McpRenderWorkflowPrompt(const FString& Name, const TMap<FString, FString>& Args);

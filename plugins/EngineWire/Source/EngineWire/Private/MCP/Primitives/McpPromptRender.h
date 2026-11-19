// McpPromptRender.h
// prompts/list entries and the prompts/get render.
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

/** manage_tools get_playbook: one domain playbook as data (title, traps, steps with their capability ids); an unknown
 * name answers PLAYBOOK_NOT_FOUND with the names there are. */
TSharedPtr<FJsonObject> McpPlaybookReply(const FString& Name);

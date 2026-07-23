// McpPromptCatalog.h
// Task 32 primitive C1 (native mirror): user-selected workflow prompts. This
// header is the native counterpart of the TypeScript module
// `src/server/mcp-primitives/prompts/`. It carries NO transport wiring, executes
// NOTHING, stores NO state, and never interpolates a secret; it defines the
// bounded prompt data shapes, the closed six-workflow allowlist, the strict
// argument kinds, the byte/length budgets, the secret guard fragments, and the
// typed error codes. Task 37 owns wiring these into `prompts/*` protocol
// methods. The definition table lives in McpPromptCatalog.cpp.
#pragma once

#include "CoreMinimal.h"

// Bounded budgets. Mirror the TypeScript MAX_PROMPT_BYTES / MAX_ARGUMENT_LENGTH.
inline constexpr int32 McpMaxPromptBytes = 65536;
inline constexpr int32 McpMaxPromptArgumentLength = 512;

// Typed, non-executing prompt error codes. Mirror the TypeScript
// PROMPT_ERROR_CODES so a rejected prompt is never mistaken for a rendered one.
namespace McpPromptErrorCodes
{
	inline const TCHAR* NotFound = TEXT("PROMPT_NOT_FOUND");
	inline const TCHAR* UnknownArgument = TEXT("PROMPT_UNKNOWN_ARGUMENT");
	inline const TCHAR* MissingArgument = TEXT("PROMPT_MISSING_ARGUMENT");
	inline const TCHAR* InvalidArgument = TEXT("PROMPT_INVALID_ARGUMENT");
	inline const TCHAR* SecretArgument = TEXT("PROMPT_SECRET_ARGUMENT");
	inline const TCHAR* ArgumentTooLong = TEXT("PROMPT_ARGUMENT_TOO_LONG");
	inline const TCHAR* TooLarge = TEXT("PROMPT_TOO_LARGE");
	inline const TCHAR* UnknownCapability = TEXT("PROMPT_UNKNOWN_CAPABILITY");
	inline const TCHAR* UnknownResource = TEXT("PROMPT_UNKNOWN_RESOURCE");
}

// A strictly typed prompt argument. `Kind` drives boundary validation; `Allowed`
// is used only for the `enum` kind. `Description` is the MCP-visible argument
// description returned by prompts/list. Arguments are never secrets or host paths.
struct FMcpPromptArgumentSpec
{
	FString Name;
	FString Kind;
	bool bRequired = false;
	FString Description;
	TArray<FString> Allowed;
};

// One workflow step: a human `Summary`, exactly one canonical capability, the
// parent tool + legacy action used to `describe` it, an optional Task 31 resource
// uri to read, and a human `Safety` note. Mirror the TypeScript PromptStep.
struct FMcpPromptStep
{
	FString Summary;
	FString CapabilityId;
	FString ParentTool;
	FString Action;
	FString ResourceUri;
	FString Safety;
};

// A versioned, immutable user-selected workflow prompt definition. `Description`
// is the MCP-visible prompt description returned by prompts/list and prompts/get.
struct FMcpWorkflowPrompt
{
	FString Id;
	int32 Version = 1;
	FString Title;
	FString Description;
	TArray<FMcpPromptArgumentSpec> Arguments;
	TArray<FMcpPromptStep> Steps;
};

// The catalog data table (defined in the .cpp), the closed id allowlist guard,
// and the secret-name guard. Mirror the TypeScript catalog and secret guard.
const TArray<FMcpWorkflowPrompt>& McpWorkflowPrompts();
const TArray<FString>& McpWorkflowPromptIds();
bool McpIsWorkflowPromptId(const FString& Name);
bool McpPromptArgumentNamesSecret(const FString& ArgumentName);

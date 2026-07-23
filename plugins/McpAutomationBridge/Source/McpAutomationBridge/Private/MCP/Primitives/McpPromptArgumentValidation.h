// McpPromptArgumentValidation.h
// Task 38 lane B: strict per-kind prompt-argument validation, split out of
// McpPromptRender to keep each translation unit within the plugin line ceiling.
// Mirrors prompt-errors.ts validateArgumentValue and the secret-value guard: a
// rejected argument yields a typed PROMPT_* code and never reaches the render.
#pragma once

#include "CoreMinimal.h"

struct FMcpPromptArgumentSpec;

bool McpPromptValueLooksSecret(const FString& Value);

bool McpValidatePromptArgument(const FMcpPromptArgumentSpec& Spec, const FString& Value, FString& OutCode, FString& OutMsg);

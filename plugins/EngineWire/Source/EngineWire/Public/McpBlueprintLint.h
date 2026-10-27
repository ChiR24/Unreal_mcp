// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"

class UBlueprint;

/** One lint_blueprint finding, as the Data Validation validator (module EngineWireValidation) reports it. */
struct FMcpLintIssue
{
	FString Rule;
	/** "error" (a While Loop that never ends) or "warning". */
	FString Severity;
	FString Graph;
	FString Node;
	FString Detail;
};

/** Every lint_blueprint rule over the Blueprint's graphs that run (events, functions, what they collapse). Reads only. */
ENGINEWIRE_API TArray<FMcpLintIssue> McpLintBlueprint(const UBlueprint* Blueprint);

/** How every message of the lint validator begins, so a reader of Data Validation results can tell them apart. */
#define MCP_LINT_MESSAGE_PREFIX TEXT("EngineWire lint ")

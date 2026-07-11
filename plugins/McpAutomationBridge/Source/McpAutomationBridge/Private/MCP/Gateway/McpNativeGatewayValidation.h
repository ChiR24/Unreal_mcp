// McpNativeGatewayValidation.h — execute validation + resolution for the unreal gateway

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

class FMcpToolRegistry;
class FMcpDynamicToolManager;

/**
 * Validates and resolves a gateway execute request.
 *
 * Returns an error envelope (success=false) to send back as a tool result, or
 * nullptr on success (OutDispatchAction + OutArguments populated). Rejects:
 *  - unknown canonical tool / action
 *  - disabled tool
 *  - params that override 'action' or 'subAction'
 *  - any param key not declared by the target tool's input schema
 */
TSharedPtr<FJsonObject> ValidateAndResolveGatewayExecute(
	const FString& ToolName, const FString& Action,
	const TSharedPtr<FJsonObject>& RawParams,
	const FMcpToolRegistry& Registry, const FMcpDynamicToolManager& ToolManager,
	FString& OutDispatchAction, TSharedPtr<FJsonObject>& OutArguments);

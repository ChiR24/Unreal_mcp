// McpNativeGatewayValidation.cpp — execute validation + resolution for the unreal gateway

#include "MCP/Gateway/McpNativeGatewayValidation.h"
#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "MCP/Registry/McpToolRegistry.h"
#include "MCP/DynamicTools/McpDynamicToolManager.h"
#include "MCP/Registry/McpToolDefinition.h"

TSharedPtr<FJsonObject> ValidateAndResolveGatewayExecute(
	const FString& ToolName, const FString& Action,
	const TSharedPtr<FJsonObject>& RawParams,
	const FMcpToolRegistry& Registry, const FMcpDynamicToolManager& ToolManager,
	FString& OutDispatchAction, TSharedPtr<FJsonObject>& OutArguments)
{
	FMcpToolDefinition* Tool = Registry.FindTool(ToolName);
	if (!Tool)
	{
		return GatewayError(TEXT("execute"), TEXT("UNKNOWN_TOOL"),
			TEXT("Unknown tool. Call search before execute."));
	}

	const TArray<FString> Actions = GatewayGetActionValues(Tool);
	if (Action.IsEmpty() || !Actions.Contains(Action))
	{
		TSharedPtr<FJsonObject> Err = GatewayError(TEXT("execute"), TEXT("UNKNOWN_ACTION"),
			FString::Printf(TEXT("Unknown action for %s. Call describe before execute."), *ToolName));
		Err->SetStringField(TEXT("tool"), ToolName);
		Err->SetArrayField(TEXT("availableActions"), GatewayStringArray(Actions));
		return Err;
	}

	if (!ToolManager.IsToolEnabled(ToolName))
	{
		return GatewayError(TEXT("execute"), TEXT("TOOL_DISABLED"),
			FString::Printf(TEXT("Tool '%s' is disabled or unavailable."), *ToolName));
	}

	if (!RawParams.IsValid())
	{
		return GatewayError(TEXT("execute"), TEXT("INVALID_PARAMS"),
			TEXT("params must be an object."));
	}

	// The gateway owns action/subAction; reject any attempt to override them.
	if (RawParams->HasField(TEXT("action")) || RawParams->HasField(TEXT("subAction")))
	{
		return GatewayError(TEXT("execute"), TEXT("INVALID_PARAMS"),
			TEXT("params must not override action or subAction. Supply the selected action at the gateway level."));
	}

	// Reject any parameter key the target tool's schema does not declare.
	const TArray<FString> Allowed = GatewayGetParameterNames(Tool);
	TArray<FString> Unknown;
	for (const auto& Pair : RawParams->Values)
	{
		if (!Allowed.Contains(Pair.Key))
		{
			Unknown.Add(Pair.Key);
		}
	}
	if (Unknown.Num() > 0)
	{
		TSharedPtr<FJsonObject> Err = GatewayError(TEXT("execute"), TEXT("UNDECLARED_PARAMETER"),
			FString::Printf(TEXT("Unknown parameter(s) for %s: %s. Call describe before execution."),
				*ToolName, *FString::Join(Unknown, TEXT(", "))));
		Err->SetArrayField(TEXT("allowedParameters"), GatewayStringArray(Allowed));
		return Err;
	}

	// Inject only 'action' here. 'subAction' is NOT a declared schema property, so
	// injecting it before the caller's strict ValidateToolArguments pass would
	// reject the 5 strict tools with UNKNOWN_TOOL_ARGUMENT. The caller injects
	// 'subAction' after validation, mirroring HandleToolsCall (validate, then alias).
	TSharedPtr<FJsonObject> Args = MakeShared<FJsonObject>();
	Args->Values = RawParams->Values;
	Args->SetStringField(TEXT("action"), Action);

	OutDispatchAction = Tool->UsesToolNameDispatch() ? ToolName : Action;
	OutArguments = Args;
	return nullptr;
}

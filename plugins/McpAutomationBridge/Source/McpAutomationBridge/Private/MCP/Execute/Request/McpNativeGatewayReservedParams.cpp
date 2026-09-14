#include "MCP/Execute/Request/McpNativeGatewayReservedParams.h"
#include "MCP/Execute/McpNativeGatewayExecuteRequest.h"

bool McpRejectReservedParams(
	const TSharedPtr<FJsonObject>& Params, const FString& GatewayAction, FMcpSemanticError& OutError)
{
	FString ParamsAction;
	if (Params->TryGetStringField(TEXT("action"), ParamsAction))
	{
		// Describe examples echo the gateway action inside params, so a verbatim
		// copy-paste used to die here with INVALID_PARAMS even though it names
		// exactly the action being executed. A matching value carries no new
		// information: strip it and proceed. A conflicting value is still a
		// smuggling attempt and is refused.
		if (!ParamsAction.IsEmpty() && ParamsAction.Equals(GatewayAction, ESearchCase::CaseSensitive))
		{
			Params->RemoveField(TEXT("action"));
		}
		else
		{
			OutError = McpValidationError(TEXT("INVALID_PARAMS"),
				TEXT("params must not override action or subAction. "
					"Supply the selected action at the gateway level."));
			return false;
		}
	}
	if (Params->HasField(TEXT("subAction")))
	{
		OutError = McpValidationError(TEXT("INVALID_PARAMS"),
			TEXT("params must not override action or subAction. "
				"Supply the selected action at the gateway level."));
		return false;
	}
	for (const FString& Control : McpExecutionOptionKeys())
	{
		if (Params->HasField(Control))
		{
			OutError = McpOptionError(Control, McpExecutionOptionKeys(),
				FString::Printf(TEXT("Gateway control '%s' must not appear in action params"), *Control));
			return false;
		}
	}
	return true;
}

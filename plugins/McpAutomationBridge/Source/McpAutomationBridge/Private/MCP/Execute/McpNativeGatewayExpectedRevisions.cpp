// McpNativeGatewayExpectedRevisions.cpp — options.expectedRevisions parsing
//
// Separate from McpNativeGatewayExecuteRequest.cpp because that file already
// sits at the plugin's 250 pure-line ceiling. This owns exactly one thing:
// turning the client's `options.expectedRevisions` envelope into typed
// live-state pins, or refusing it with a typed error.

#include "MCP/Execute/McpNativeGatewayExecuteRequest.h"

bool McpParseExpectedRevisions(
	const TSharedPtr<FJsonObject>& Options,
	TMap<EMcpStateKind, int64>& OutRevisions,
	FMcpSemanticError& OutError)
{
	// Cleared up front and only assigned on full success: the game-thread gate
	// enforces whatever survives this call, so a half-parsed set would enforce a
	// precondition the client never actually asked for.
	OutRevisions.Empty();
	if (!Options.IsValid())
	{
		return true;
	}
	FMcpExpectedRevisionsParseResult Parsed =
		FMcpLiveStateRevisions::ParseExpectedRevisions(
			Options->TryGetField(TEXT("expectedRevisions")));
	if (Parsed.bSuccess)
	{
		OutRevisions = MoveTemp(Parsed.Revisions);
		return true;
	}
	if (Parsed.ErrorCode == TEXT("INVALID_OPTIONS"))
	{
		OutError = McpValidationError(
			Parsed.ErrorCode, Parsed.Message, Parsed.Pointer);
	}
	else if (Parsed.ErrorCode == TEXT("UNSUPPORTED_OPTION"))
	{
		OutError = McpOptionError(
			Parsed.Field, FMcpLiveStateRevisions::AllKeys(), Parsed.Message);
	}
	else
	{
		OutError = McpRangeError(Parsed.Field, Parsed.Message);
	}
	return false;
}

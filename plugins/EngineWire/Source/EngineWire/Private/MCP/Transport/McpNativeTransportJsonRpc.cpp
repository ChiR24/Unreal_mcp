#include "MCP/Transport/McpNativeTransportPrivate.h"

// ─── Tools Call (SSE streaming) ─────────────────────────────────────────────

void FMcpNativeTransport::HandleToolsCall(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id,
	FSocket* ClientSocket, const FString& SessionId, const FString& CorsOrigin)
{
	const auto RejectParams = [&](const TCHAR* Message)
	{
		SendAndClose(ClientSocket, 200, TEXT("application/json"),
			FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams, Message), {}, CorsOrigin);
	};
	if (!Params.IsValid())
	{
		RejectParams(TEXT("Missing params"));
		return;
	}

	FString ToolName;
	if (!Params->TryGetStringField(TEXT("name"), ToolName))
	{
		RejectParams(TEXT("Missing tool name"));
		return;
	}

	TSharedPtr<FJsonObject> Arguments;
	const TSharedPtr<FJsonValue> ArgsValue = Params->TryGetField(TEXT("arguments"));

	if (ArgsValue.IsValid() && ArgsValue->Type != EJson::Null)
	{
		if (ArgsValue->Type != EJson::Object)
		{
			RejectParams(TEXT("'arguments' must be an object if provided"));
			return;
		}
		Arguments = ArgsValue->AsObject();
	}

	if (!Arguments.IsValid()) Arguments = MakeShared<FJsonObject>();

	// The public surface is permanently the single static 'unreal' gateway tool.
	// Capture the client's _meta.progressToken (if any) and thread it through the
	// gateway so streamed notifications/progress echo the client's own token.
	// Pre-dispatch is total: it routes 'unreal' to the gateway and answers every
	// other (removed) direct tool name with an executable migration payload, so no
	// legacy direct-call path runs after it.
	TSharedPtr<FJsonValue> ProgressToken;
	const TSharedPtr<FJsonObject>* ProgressMeta = nullptr;
	if (Params.IsValid() && Params->TryGetObjectField(TEXT("_meta"), ProgressMeta) && ProgressMeta)
	{
		const TSharedPtr<FJsonValue>* TokenValue = (*ProgressMeta)->Values.Find(TEXT("progressToken"));
		if (TokenValue && TokenValue->IsValid())
		{
			ProgressToken = *TokenValue;
		}
	}
	HandleGatewayModePreDispatch(ToolName, Arguments, Id, ClientSocket, SessionId, CorsOrigin, ProgressToken);
}

// ─── SSE Connection Management ──────────────────────────────────────────────

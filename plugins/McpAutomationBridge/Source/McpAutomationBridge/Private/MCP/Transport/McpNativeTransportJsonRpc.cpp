#include "MCP/Transport/McpNativeTransportPrivate.h"

// Send a prebuilt JSON-RPC body and tear down the client socket. Shared by the
// early-return error paths in HandleToolsCall so each stays a one-liner.
void FMcpNativeTransport::SendBodyAndClose(FSocket* ClientSocket,
	const FString& Body, int32 Status, const FString& CorsOrigin)
{
	SendHttpResponse(ClientSocket, Status, TEXT("application/json"), Body, {}, CorsOrigin);
	ClientSocket->Close();
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSub) SocketSub->DestroySocket(ClientSocket);
}

// ─── Tools Call (SSE streaming) ─────────────────────────────────────────────

void FMcpNativeTransport::HandleToolsCall(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id,
	FSocket* ClientSocket, const FString& SessionId, const FString& CorsOrigin)
{
	if (!Params.IsValid())
	{
		SendBodyAndClose(ClientSocket,
			FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams, TEXT("Missing params")),
			200, CorsOrigin);
		return;
	}

	FString ToolName;
	if (!Params->TryGetStringField(TEXT("name"), ToolName))
	{
		SendBodyAndClose(ClientSocket,
			FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams, TEXT("Missing tool name")),
			200, CorsOrigin);
		return;
	}

	TSharedPtr<FJsonObject> Arguments;
	const TSharedPtr<FJsonValue> ArgsValue = Params->TryGetField(TEXT("arguments"));

	if (ArgsValue.IsValid() && ArgsValue->Type != EJson::Null)
	{
		if (ArgsValue->Type != EJson::Object)
		{
			SendBodyAndClose(ClientSocket,
				FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams,
					TEXT("'arguments' must be an object if provided")),
				200, CorsOrigin);
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
	if (HandleGatewayModePreDispatch(ToolName, Arguments, Id, ClientSocket, SessionId, CorsOrigin, ProgressToken)) return;
}

// ─── SSE Connection Management ──────────────────────────────────────────────

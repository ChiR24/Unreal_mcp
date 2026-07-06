#include "MCP/Transport/McpNativeTransportPrivate.h"

bool FMcpNativeTransport::TryHandleLocalToolCall(
	const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments,
	const TSharedPtr<FJsonValue>& Id, FSocket* ClientSocket,
	const FString& SessionId, const FString& CorsOrigin)
{
	if (ToolName != TEXT("manage_tools"))
	{
		return false;
	}

	ISocketSubsystem* SocketSub =
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	FString Action;
	Arguments->TryGetStringField(TEXT("action"), Action);
	{
		FScopeLock SessionLock(&SessionMutex);
		if (!ActiveSessions.Contains(SessionId))
		{
			const FString Body = FMcpJsonRpc::BuildError(
				Id, FMcpJsonRpc::ErrorInvalidRequest,
				TEXT("Invalid or expired session ID"));
			SendHttpResponse(
				ClientSocket, 404, TEXT("application/json"), Body, {},
				CorsOrigin);
			ClientSocket->Close();
			if (SocketSub)
			{
				SocketSub->DestroySocket(ClientSocket);
			}
			return true;
		}
	}
	// HandleAction runs outside the session lock so it cannot stall other
	// sessions' ValidateSession / rate-limit / progress writes. The
	// FMcpDynamicToolManager protects its own state with StateMutex, and
	// the TOCTOU window (session expiring between check and HandleAction)
	// is bounded: a session-closing teardown will not affect a completed
	// manage_tools response.
	TSharedPtr<FJsonObject> Result = ToolManager.HandleAction(Action, Arguments);

	bool bActionSuccess = false;
	if (Result.IsValid())
	{
		Result->TryGetBoolField(TEXT("success"), bActionSuccess);
	}
	FString ActionMessage = TEXT("OK");
	if (!bActionSuccess && Result.IsValid())
	{
		Result->TryGetStringField(TEXT("error"), ActionMessage);
	}
	const TSharedPtr<FJsonObject> ToolResult =
		FMcpJsonRpc::BuildToolResult(bActionSuccess, ActionMessage, Result);
	const FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
	SendHttpResponse(
		ClientSocket, 200, TEXT("application/json"), Body, {}, CorsOrigin);
	ClientSocket->Close();
	if (SocketSub)
	{
		SocketSub->DestroySocket(ClientSocket);
	}
	return true;
}

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

	FString Action;
	Arguments->TryGetStringField(TEXT("action"), Action);
	{
		FScopeLock SessionLock(&SessionMutex);
		if (!ActiveSessions.Contains(SessionId))
		{
			const FString Body = FMcpJsonRpc::BuildError(
				Id, FMcpJsonRpc::ErrorInvalidRequest,
				TEXT("Invalid or expired session ID"));
			SendAndClose(
				ClientSocket, 404, TEXT("application/json"), Body, {},
				CorsOrigin);
			return true;
		}
	}
	// HandleAction runs outside the session lock so it cannot stall other
	// sessions' ValidateSession / rate-limit / progress writes. The
	// FMcpDynamicToolManager protects its own state with StateMutex, and
	// the TOCTOU window (session expiring between check and HandleAction)
	// is bounded: a session-closing teardown will not affect a completed
	// manage_tools response.
	TSharedPtr<FJsonObject> Result;
	if (Action == TEXT("get_task_result"))
	{
		FString TaskId;
		Arguments->TryGetStringField(TEXT("taskId"), TaskId);
		Result = DescribeTask(TaskId, SessionId);
	}
	else
	{
		Result = ToolManager.HandleAction(Action, Arguments);
	}

	bool bActionSuccess = false;
	if (Result.IsValid())
	{
		Result->TryGetBoolField(TEXT("success"), bActionSuccess);
	}
	FString ActionMessage = TEXT("OK");
	FString ErrorCode;
	if (!bActionSuccess && Result.IsValid())
	{
		Result->TryGetStringField(TEXT("error"), ActionMessage);
		Result->TryGetStringField(TEXT("errorCode"), ErrorCode);
	}
	const TSharedPtr<FJsonObject> ToolResult =
		FMcpJsonRpc::BuildToolResult(bActionSuccess, ActionMessage, Result, ErrorCode);
	const FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
	SendAndClose(
		ClientSocket, 200, TEXT("application/json"), Body, {}, CorsOrigin);
	return true;
}

TSharedPtr<FJsonObject> FMcpNativeTransport::DescribeTask(const FString& TaskId, const FString& SessionId)
{
	TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Outcome;
	if (!TaskResults.Find(TaskId, GetSessionPrincipal(SessionId).Identity, Outcome))
	{
		Out->SetBoolField(TEXT("success"), false);
		Out->SetStringField(TEXT("errorCode"), TEXT("TASK_NOT_FOUND"));
		Out->SetStringField(TEXT("error"), FString::Printf(
			TEXT("No task %s: only a call that answered \"still running\" since the editor started is kept, the last %d of them."),
			*TaskId, FMcpTaskResults::MaxKept));
		return Out;
	}
	Out->SetBoolField(TEXT("success"), true);
	Out->SetStringField(TEXT("taskId"), TaskId);
	if (Outcome.IsValid())
	{
		Out->SetStringField(TEXT("state"), TEXT("done"));
		Out->SetObjectField(TEXT("outcome"), Outcome);
		Out->SetStringField(TEXT("message"), FString::Printf(TEXT("Task %s is done."), *TaskId));
		return Out;
	}
	float Percent = 0.0f;
	{
		FScopeLock Lock(&SSEConnectionsMutex);
		const TSharedPtr<FSSEConnection>* Found = SSEConnections.Find(TaskId);
		if (Found && Found->IsValid())
		{
			Percent = (*Found)->LastProgressPercent;
		}
	}
	const bool bWaiting = Subsystem && Subsystem->IsAutomationRequestWaiting(TaskId);
	Out->SetStringField(TEXT("state"), bWaiting ? TEXT("queued") : TEXT("running"));
	if (Percent > 0.0f)
	{
		Out->SetNumberField(TEXT("progress"), FMath::Min(Percent, 100.0f) / 100.0f);
	}
	Out->SetStringField(TEXT("message"), FString::Printf(TEXT("Task %s is %s."), *TaskId, bWaiting ? TEXT("queued") : TEXT("still running")));
	return Out;
}

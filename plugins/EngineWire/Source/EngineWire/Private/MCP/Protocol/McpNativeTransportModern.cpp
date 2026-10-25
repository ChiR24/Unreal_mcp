// McpNativeTransportModern.cpp — MCP 2026-07-28 requests on the native /mcp transport. Kept with the protocol
// helpers because MCP/Transport is at its folder cap; these are FMcpNativeTransport members all the same.
//
// A modern request carries its protocol version and client capabilities in params._meta and mirrors its method and
// name into headers; there is no initialize and no Mcp-Session-Id. Each one is served on an implicit session per
// principal, so the rate limits, pending-call caps and principal checks of the session path apply unchanged.

#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Execute/McpNativeGatewayAuthorization.h"
#include "MCP/Protocol/McpModernRequest.h"
#include "Misc/SecureHash.h"

void FMcpNativeTransport::HandleModernRequest(
	FSocket* ClientSocket, const FParsedHttpRequest& HttpReq, const FMcpJsonRpcRequest& Rpc)
{
	const auto Reply = [&](int32 Status, const FString& Body)
	{
		SendAndClose(ClientSocket, Status, TEXT("application/json"), Body, {}, HttpReq.Origin);
	};
	const McpModern::FMeta Meta = McpModern::ReadMeta(Rpc.Params);
	if (Meta.ProtocolVersion != McpModern::Version)
	{
		// Only the versions a request can name here, as the TypeScript SDK answers; initialize negotiates the rest.
		Reply(400, McpModern::UnsupportedVersionError(Rpc.Id, Meta.ProtocolVersion, {FString(McpModern::Version)}));
		return;
	}
	const FString Mismatch = McpModern::HeaderMismatch(
		HttpReq.ProtocolVersion, HttpReq.McpMethod, HttpReq.McpName, Rpc.Method, Rpc.Params, Meta.ProtocolVersion);
	if (!Mismatch.IsEmpty())
	{
		Reply(400, FMcpJsonRpc::BuildError(Rpc.Id, McpModern::ErrorHeaderMismatch, Mismatch));
		return;
	}
	if (Rpc.bIsNotification)
	{
		// This revision defines no client notification over HTTP: closing a call's stream is what cancels it.
		SendAndClose(ClientSocket, 202, TEXT("text/plain"), FString(), {}, HttpReq.Origin);
		return;
	}

	const FString SessionId = EnsureModernSession(HttpReq.CapabilityToken);
	FString RateLimitError;
	int32 RetryAfter = 0;
	if (!ConsumeSessionRequestBudget(SessionId, Rpc.Method == TEXT("tools/call"), RateLimitError, &RetryAfter))
	{
		SendAndClose(ClientSocket, 429, TEXT("application/json"), FMcpJsonRpc::BuildRateLimitError(Rpc.Id, RateLimitError, RetryAfter),
			FMcpJsonRpc::RetryAfterHeader(RetryAfter), HttpReq.Origin);
		return;
	}

	McpModern::FCall Call;
	Call.bActive = true;
	Call.bTasks = Meta.bTasks;
	Call.bElicitationForm = Meta.bElicitationForm;
	Call.Method = Rpc.Method;
	const TSharedPtr<FJsonObject>* Responses = nullptr;
	if (Rpc.Params->TryGetObjectField(TEXT("inputResponses"), Responses) && Responses)
	{
		Call.InputResponses = *Responses;
	}
	Rpc.Params->TryGetStringField(TEXT("requestState"), Call.RequestState);
	const McpModern::FScope Scope(Call);

	if (Rpc.Method == TEXT("server/discover"))
	{
		Reply(200, HandleServerDiscover(Rpc.Id));
		return;
	}
	if (Rpc.Method == TEXT("tools/list"))
	{
		Reply(200, HandleToolsList(Rpc.Id, SessionId));
		return;
	}
	if (Rpc.Method == TEXT("tools/call"))
	{
		// Streams or answers on the socket itself, like the session path.
		HandleToolsCall(Rpc.Params, Rpc.Id, ClientSocket, SessionId, HttpReq.Origin);
		return;
	}
	if (Rpc.Method == TEXT("tasks/get") || Rpc.Method == TEXT("tasks/cancel") || Rpc.Method == TEXT("tasks/update"))
	{
		FString CancelId;
		Reply(200, HandleTaskMethod(Rpc.Method, Rpc.Params, Rpc.Id, SessionId, CancelId));
		// Only after the answer: cancelling waits on the request queue's execution barrier.
		if (!CancelId.IsEmpty() && Subsystem)
		{
			Subsystem->CancelAutomationRequest(CancelId);
		}
		return;
	}
	if (HandlePrimitiveMethod(Rpc.Method, Rpc.Params, Rpc.Id, ClientSocket, SessionId, HttpReq.Origin))
	{
		return;
	}
	// ping, logging/setLevel and initialize are gone in this revision, so they land here with any unknown method.
	Reply(404, FMcpJsonRpc::BuildError(Rpc.Id, FMcpJsonRpc::ErrorMethodNotFound,
		FString::Printf(TEXT("Unknown method: %s"), *Rpc.Method)));
}

FString FMcpNativeTransport::EnsureModernSession(const FString& PresentedToken)
{
	// The id is internal and never sent; salted per editor run so no client can name it in Mcp-Session-Id.
	static const FString Salt = FGuid::NewGuid().ToString();
	const FMcpCapabilityPrincipal Principal = McpResolveNativePrincipal(PresentedToken);
	const FString SessionId = TEXT("modern-") + FMD5::HashAnsiString(*(Salt + Principal.Identity));
	// ponytail: one implicit session per principal, so modern clients sharing a token share its rate and
	// pending-call limits; key it by clientInfo too if they need separate budgets.
	FScopeLock Lock(&SessionMutex);
	if (!ActiveSessions.Contains(SessionId))
	{
		const double Now = FPlatformTime::Seconds();
		ActiveSessions.Add(SessionId, Now);
		FSessionRateState RateState;
		RateState.ClientRateKey = SessionId;
		RateState.InitializationCompletedAt = Now;
		SessionRateStates.Add(SessionId, RateState);
		SessionPrincipals.Add(SessionId, Principal);
	}
	return SessionId;
}

FString FMcpNativeTransport::HandleTaskMethod(const FString& Method, const TSharedPtr<FJsonObject>& Params,
	const TSharedPtr<FJsonValue>& Id, const FString& SessionId, FString& OutCancelId)
{
	FString TaskId;
	Params->TryGetStringField(TEXT("taskId"), TaskId);
	FMcpTaskResults::FTask Task;
	if (!TaskResults.Find(TaskId, GetSessionPrincipal(SessionId).Identity, Task))
	{
		return FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams, FString::Printf(
			TEXT("No task %s: only a call that answered with a task since the editor started is kept, the last %d of them."),
			*TaskId, FMcpTaskResults::MaxKept));
	}
	if (Method == TEXT("tasks/update"))
	{
		return FMcpJsonRpc::BuildError(Id, FMcpJsonRpc::ErrorInvalidParams,
			FString::Printf(TEXT("Task %s is not waiting for input."), *TaskId));
	}
	if (Method == TEXT("tasks/cancel"))
	{
		// Only a call still waiting in the queue can be stopped: one already running finishes the step it is in, and
		// tasks/get then reports its real result rather than a cancel that did not happen.
		// ponytail: a call that starts between this check and the cancel is cancelled after it ran, as with
		// notifications/cancelled; close the gap in the queue if it ever matters.
		if (!Task.Outcome.IsValid() && !Task.bCancelled && Subsystem && Subsystem->IsAutomationRequestWaiting(TaskId))
		{
			FScopeLock Lock(&SSEConnectionsMutex);
			const TSharedPtr<FSSEConnection>* Found = SSEConnections.Find(TaskId);
			if (Found && Found->IsValid())
			{
				(*Found)->bCancelled.store(true);
				(*Found)->bMarkedForRemoval.store(true);
				OutCancelId = TaskId;
			}
		}
		if (!OutCancelId.IsEmpty())
		{
			TaskResults.NoteCancelled(TaskId);
		}
		TSharedPtr<FJsonObject> Ack = MakeShared<FJsonObject>();
		Ack->SetStringField(TEXT("resultType"), TEXT("complete"));
		return FMcpJsonRpc::BuildResponse(Id, Ack);
	}

	FString Status = TEXT("working");
	FString Message;
	if (Task.Outcome.IsValid())
	{
		Status = TEXT("completed");
	}
	else if (Task.bCancelled)
	{
		Status = TEXT("cancelled");
	}
	else
	{
		const FString Work = EngineWire::DescribeEditorWork(TaskId);
		Message = Subsystem && Subsystem->IsAutomationRequestWaiting(TaskId)
			? FString::Printf(TEXT("Queued: Unreal is %s"), Work.IsEmpty() ? TEXT("busy") : *Work)
			: Work;
	}
	TSharedPtr<FJsonObject> Result = McpModern::TaskObject(
		TaskId, Status, Message, Task.CreatedAt, Task.UpdatedAt, TEXT("complete"));
	if (Task.Outcome.IsValid() && Task.ToolResult.IsValid())
	{
		Result->SetObjectField(TEXT("result"), Task.ToolResult);
	}
	return FMcpJsonRpc::BuildResponse(Id, Result);
}

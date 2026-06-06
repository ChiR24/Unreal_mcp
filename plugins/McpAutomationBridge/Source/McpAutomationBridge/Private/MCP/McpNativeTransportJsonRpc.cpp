#include "MCP/McpNativeTransportPrivate.h"

FString FMcpNativeTransport::HandleInitialize(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id,
	FString& OutSessionId)
{
	// Extract client info for logging
	FString ClientName = TEXT("unknown");
	FString ClientVersion = TEXT("unknown");
	if (Params.IsValid())
	{
		const TSharedPtr<FJsonObject>* ClientInfoObj = nullptr;
		if (Params->TryGetObjectField(TEXT("clientInfo"), ClientInfoObj) && ClientInfoObj)
		{
			(*ClientInfoObj)->TryGetStringField(TEXT("name"), ClientName);
			(*ClientInfoObj)->TryGetStringField(TEXT("version"), ClientVersion);
		}
	}

	OutSessionId = FGuid::NewGuid().ToString();
	int32 CurrentSessionCount;
	{
		FScopeLock Lock(&SessionMutex);
		ActiveSessions.Add(OutSessionId, FPlatformTime::Seconds());
		CurrentSessionCount = ActiveSessions.Num();
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("protocolVersion"), TEXT("2025-03-26"));

	auto Capabilities = MakeShared<FJsonObject>();
	auto ToolsCap = MakeShared<FJsonObject>();
	ToolsCap->SetBoolField(TEXT("listChanged"), true);
	Capabilities->SetObjectField(TEXT("tools"), ToolsCap);
	Result->SetObjectField(TEXT("capabilities"), Capabilities);

	auto ServerInfo = MakeShared<FJsonObject>();
	ServerInfo->SetStringField(TEXT("name"), ServerName);
	ServerInfo->SetStringField(TEXT("version"), ServerVersion);
	Result->SetObjectField(TEXT("serverInfo"), ServerInfo);

	// Combine base instructions (from server-info.json) + user instructions (from settings)
	FString CombinedInstructions = BaseInstructions;
	if (!UserInstructions.IsEmpty())
	{
		if (!CombinedInstructions.IsEmpty())
		{
			CombinedInstructions += TEXT("\n\n");
		}
		CombinedInstructions += UserInstructions;
	}
	if (!CombinedInstructions.IsEmpty())
	{
		Result->SetStringField(TEXT("instructions"), CombinedInstructions);
	}

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("MCP session initialized: %s (client: %s %s, active sessions: %d)"),
		*OutSessionId, *ClientName, *ClientVersion, CurrentSessionCount);

	return FMcpJsonRpc::BuildResponse(Id, Result);
}

// ─── Tools List ─────────────────────────────────────────────────────────────

FString FMcpNativeTransport::HandleToolsList(const TSharedPtr<FJsonValue>& Id)
{
	TSet<FString> EnabledTools = ToolManager.GetEnabledToolNames();
	TSharedPtr<FJsonObject> ToolsList = FMcpToolRegistry::Get().GetFilteredToolsResponse(EnabledTools);

	if (ToolsList.IsValid())
	{
		return FMcpJsonRpc::BuildResponse(Id, ToolsList);
	}

	auto EmptyResult = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> EmptyArray;
	EmptyResult->SetArrayField(TEXT("tools"), EmptyArray);
	return FMcpJsonRpc::BuildResponse(Id, EmptyResult);
}

int32 FMcpNativeTransport::GetTotalToolCount() const
{
	return FMcpToolRegistry::Get().GetToolCount();
}

// ─── Tools Call (SSE streaming) ─────────────────────────────────────────────

void FMcpNativeTransport::HandleToolsCall(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id,
	FSocket* ClientSocket, const FString& SessionId, const FString& CorsOrigin)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	if (!Params.IsValid())
	{
		FString ErrorBody = FMcpJsonRpc::BuildError(
			Id, FMcpJsonRpc::ErrorInvalidParams, TEXT("Missing params"));
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ErrorBody, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	FString ToolName;
	if (!Params->TryGetStringField(TEXT("name"), ToolName))
	{
		FString ErrorBody = FMcpJsonRpc::BuildError(
			Id, FMcpJsonRpc::ErrorInvalidParams, TEXT("Missing tool name"));
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ErrorBody, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	TSharedPtr<FJsonObject> Arguments;
	const TSharedPtr<FJsonValue> ArgsValue = Params->TryGetField(TEXT("arguments"));

	if (ArgsValue.IsValid() && ArgsValue->Type != EJson::Null)
	{
		if (ArgsValue->Type != EJson::Object)
		{
			FString ErrorBody = FMcpJsonRpc::BuildError(
				Id, FMcpJsonRpc::ErrorInvalidParams,
				TEXT("'arguments' must be an object if provided"));
			SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ErrorBody, {}, CorsOrigin);
			ClientSocket->Close();
			if (SocketSub) SocketSub->DestroySocket(ClientSocket);
			return;
		}
		Arguments = ArgsValue->AsObject();
	}

	if (!Arguments.IsValid())
	{
		Arguments = MakeShared<FJsonObject>();
	}

	// Intercept manage_tools — handle locally (one-shot, no SSE)
	if (ToolName == TEXT("manage_tools"))
	{
		FString Action;
		Arguments->TryGetStringField(TEXT("action"), Action);
		TSharedPtr<FJsonObject> Result = ToolManager.HandleAction(Action, Arguments);
		bool bActionSuccess = false;
		if (Result.IsValid()) { Result->TryGetBoolField(TEXT("success"), bActionSuccess); }
		FString ActionMessage = TEXT("OK");
		if (!bActionSuccess && Result.IsValid())
		{
			Result->TryGetStringField(TEXT("error"), ActionMessage);
		}
		TSharedPtr<FJsonObject> ToolResult = FMcpJsonRpc::BuildToolResult(
			bActionSuccess, ActionMessage, Result);
		FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), Body, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Enforce tool enabled check — tools/list filters, tools/call must also enforce
	if (!ToolManager.IsToolEnabled(ToolName))
	{
		TSharedPtr<FJsonObject> ToolResult = FMcpJsonRpc::BuildToolResult(
			false,
			FString::Printf(TEXT("Tool '%s' is not enabled"), *ToolName),
			nullptr, TEXT("TOOL_DISABLED"));
		FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), Body, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Send SSE headers — begins the streaming response
	if (!SendSSEHeaders(ClientSocket, SessionId, CorsOrigin))
	{
		UE_LOG(LogMcpNativeTransport, Warning,
			TEXT("Failed to send SSE headers for tool %s"), *ToolName);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
		return;
	}

	// Generate request ID and park the connection
	const FString RequestId = FGuid::NewGuid().ToString();

	{
		FScopeLock Lock(&SSEConnectionsMutex);
		TSharedPtr<FSSEConnection> Conn = MakeShared<FSSEConnection>();
		Conn->Socket = ClientSocket;
		Conn->JsonRpcId = Id;
		Conn->StartTime = FPlatformTime::Seconds();
		Conn->ToolName = ToolName;
		Conn->SessionId = SessionId;
		SSEConnections.Add(RequestId, Conn);
	}

	UE_LOG(LogMcpNativeTransport, Log,
		TEXT("tools/call: %s (RequestId=%s)"),
		*ToolName, *RequestId);

	// Resolve dispatch action using tool definition metadata.
	// Pattern A: pass tool name as Action (handler checks Action == "tool_name")
	// Pattern B: extract sub-action from arguments (handler checks Action.StartsWith("sub_action"))
	FString DispatchAction = ToolName;
	FMcpToolDefinition* ToolDef = FMcpToolRegistry::Get().FindTool(ToolName);
	if (ToolDef && !ToolDef->UsesToolNameDispatch())
	{
		// Pattern B: extract action from arguments
		FString ActionField = ToolDef->GetActionFieldName();
		FString Extracted;
		if (Arguments->TryGetStringField(ActionField, Extracted) && !Extracted.IsEmpty())
		{
			DispatchAction = Extracted;
		}
		else
		{
			CompletePendingRequest(
				RequestId, false,
				FString::Printf(TEXT("Missing required '%s' field in arguments for tool '%s'"),
					*ActionField, *ToolName),
				nullptr, TEXT("INVALID_PARAMS"));
			return;
		}
	}

	// Normalize: some handlers read "subAction" instead of "action".
	// Ensure both fields exist so handlers find the value regardless of field name.
	if (!Arguments->HasField(TEXT("subAction")) && Arguments->HasField(TEXT("action")))
	{
		FString ActionVal;
		Arguments->TryGetStringField(TEXT("action"), ActionVal);
		Arguments->SetStringField(TEXT("subAction"), ActionVal);
	}

	// Dispatch through the subsystem queue. The queue is drained by the core
	// ticker after world ticking, which is required for safe map transitions.
	TWeakObjectPtr<UMcpAutomationBridgeSubsystem> WeakSubsystem(Subsystem);
	FString CapturedRequestId = RequestId;
	FString CapturedDispatchAction = DispatchAction;
	TSharedPtr<FJsonObject> CapturedArguments = Arguments;

	if (UMcpAutomationBridgeSubsystem* Sub = WeakSubsystem.Get())
	{
		Sub->QueueAutomationRequest(
			CapturedRequestId, CapturedDispatchAction, CapturedArguments, nullptr,
			ERequestOrigin::NativeHTTP);
	}
}

// ─── SSE Connection Management ──────────────────────────────────────────────

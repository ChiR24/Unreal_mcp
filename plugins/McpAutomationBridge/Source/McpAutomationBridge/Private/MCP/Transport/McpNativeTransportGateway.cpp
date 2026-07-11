// McpNativeTransportGateway.cpp — route tools/call for the 'unreal' gateway tool

#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Transport/McpNativeTransportArgumentValidation.h"
#include "MCP/Gateway/McpNativeGatewayDefinition.h"
#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "MCP/Gateway/McpNativeGatewayValidation.h"

void FMcpNativeTransport::HandleGatewayCall(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id,
	FSocket* ClientSocket, const FString& SessionId, const FString& CorsOrigin,
	const TSharedPtr<FJsonValue>& ProgressToken)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	auto SendOneShot = [&](const TSharedPtr<FJsonObject>& ToolResult, int32 Status = 200)
	{
		const FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
		SendHttpResponse(ClientSocket, Status, TEXT("application/json"), Body, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
	};

	if (!Params.IsValid())
	{
		SendOneShot(FMcpJsonRpc::BuildToolResult(false,
			TEXT("operation must be search, describe, execute, or configure."),
			nullptr, TEXT("UNKNOWN_OPERATION")));
		return;
	}

	FString Operation;
	if (!Params->TryGetStringField(TEXT("operation"), Operation) || Operation.IsEmpty())
	{
		SendOneShot(FMcpJsonRpc::BuildToolResult(false,
			TEXT("operation must be search, describe, execute, or configure."),
			nullptr, TEXT("UNKNOWN_OPERATION")));
		return;
	}

	// Every gateway operation requires a valid session.
	{
		FScopeLock SessionLock(&SessionMutex);
		if (!ActiveSessions.Contains(SessionId))
		{
			const FString Body = FMcpJsonRpc::BuildError(
				Id, FMcpJsonRpc::ErrorInvalidRequest,
				TEXT("Invalid or expired session ID"));
			SendHttpResponse(ClientSocket, 404, TEXT("application/json"), Body, {}, CorsOrigin);
			ClientSocket->Close();
			if (SocketSub) SocketSub->DestroySocket(ClientSocket);
			return;
		}
	}

	const FMcpToolRegistry& Registry = FMcpToolRegistry::Get();

	if (Operation == TEXT("search"))
	{
		FString Query;
		Params->TryGetStringField(TEXT("query"), Query);
		int32 Limit = 12;
		if (Params->HasField(TEXT("limit")))
		{
			int32 L = 0;
			if (Params->TryGetNumberField(TEXT("limit"), L)) Limit = FMath::Clamp(L, 1, 25);
		}
		int32 Offset = 0;
		if (Params->HasField(TEXT("offset")))
		{
			int32 O = 0;
			if (Params->TryGetNumberField(TEXT("offset"), O)) Offset = FMath::Max(0, O);
		}
		TSharedPtr<FJsonObject> Result = SearchGatewayCatalog(Query, Limit, Offset, Registry, ToolManager);
		SendOneShot(FMcpJsonRpc::BuildToolResult(true, TEXT("ok"), Result));
		return;
	}

	if (Operation == TEXT("describe"))
	{
		FString Tool;
		Params->TryGetStringField(TEXT("tool"), Tool);
		FString Action;
		Params->TryGetStringField(TEXT("action"), Action);
		TSharedPtr<FJsonObject> Result = DescribeGatewayCapability(Tool, Action, Registry, ToolManager);
		bool bOk = false;
		if (Result.IsValid()) Result->TryGetBoolField(TEXT("success"), bOk);
		const FString Msg = bOk ? TEXT("ok")
			: (Result.IsValid() ? Result->GetStringField(TEXT("message")) : TEXT("describe failed"));
		const FString Code = bOk ? FString()
			: (Result.IsValid() ? Result->GetStringField(TEXT("errorCode")) : FString());
		SendOneShot(FMcpJsonRpc::BuildToolResult(bOk, Msg, Result, Code));
		return;
	}

	if (Operation == TEXT("configure"))
	{
		FString Action;
		Params->TryGetStringField(TEXT("action"), Action);
		if (Action.IsEmpty())
		{
			SendOneShot(FMcpJsonRpc::BuildToolResult(false,
				TEXT("configure requires a manage_tools action."), nullptr, TEXT("MISSING_ACTION")));
			return;
		}
		TSharedPtr<FJsonObject> ManageArgs = MakeShared<FJsonObject>();
		const TSharedPtr<FJsonObject>* Nested = nullptr;
		if (Params->TryGetObjectField(TEXT("params"), Nested) && *Nested)
		{
			ManageArgs->Values = (*Nested)->Values;
		}
		ManageArgs->SetStringField(TEXT("action"), Action);
		TSharedPtr<FJsonObject> Result = ToolManager.HandleAction(Action, ManageArgs);
		bool bOk = false;
		if (Result.IsValid()) Result->TryGetBoolField(TEXT("success"), bOk);
		const FString Msg = bOk ? TEXT("ok")
			: (Result.IsValid() ? Result->GetStringField(TEXT("error")) : TEXT("configure failed"));
		SendOneShot(FMcpJsonRpc::BuildToolResult(bOk, Msg, Result));
		return;
	}

	if (Operation == TEXT("execute"))
	{
		FString Tool;
		Params->TryGetStringField(TEXT("tool"), Tool);
		FString Action;
		Params->TryGetStringField(TEXT("action"), Action);
		TSharedPtr<FJsonObject> RawParams;
		const TSharedPtr<FJsonObject>* Nested = nullptr;
		if (Params->TryGetObjectField(TEXT("params"), Nested) && *Nested)
		{
			RawParams = *Nested;
		}
		else
		{
			RawParams = MakeShared<FJsonObject>();
		}

		FString DispatchAction;
		TSharedPtr<FJsonObject> ResolvedArgs;
		TSharedPtr<FJsonObject> Err = ValidateAndResolveGatewayExecute(
			Tool, Action, RawParams, Registry, ToolManager, DispatchAction, ResolvedArgs);
		if (Err.IsValid())
		{
			bool bOk = false;
			Err->TryGetBoolField(TEXT("success"), bOk);
			const FString Msg = Err->GetStringField(TEXT("message"));
			const FString Code = Err->GetStringField(TEXT("errorCode"));
			SendOneShot(FMcpJsonRpc::BuildToolResult(bOk, Msg, Err, Code));
			return;
		}

		// Let locally-handled tools (e.g. manage_tools) complete without
		// queueing through the subsystem. Run this BEFORE ValidateToolArguments
		// so locally-intercepted strict tools are not rejected by the schema
		// pass (mirrors HandleToolsCall ordering: :71-75 local -> :79-98 validate).
		if (TryHandleLocalToolCall(
				Tool, ResolvedArgs, Id, ClientSocket, SessionId, CorsOrigin))
		{
			return;
		}

		// Validate required fields and value types against the canonical tool
		// schema before queueing. The gateway's own validation only checks
		// tool/action existence and param-key whitelist; this catches missing
		// required fields and type mismatches upfront (matching the non-gateway
		// tools/call path in McpNativeTransportJsonRpc.cpp).
		FMcpToolDefinition* ToolDef = Registry.FindTool(Tool);
		FString ArgPath, ArgErrorCode, ArgErrorMessage;
		if (!McpNativeArgumentValidation::ValidateToolArguments(
				ToolDef, ResolvedArgs, ArgPath, ArgErrorCode, ArgErrorMessage))
		{
			const FString Message = ArgErrorMessage.IsEmpty()
				? FString::Printf(TEXT("Tool '%s' could not validate its arguments"), *Tool)
				: ArgErrorMessage;
			SendOneShot(FMcpJsonRpc::BuildToolResult(false, Message, nullptr,
				ArgErrorCode.IsEmpty() ? TEXT("INVALID_TOOL_ARGUMENT") : ArgErrorCode));
			return;
		}

		// Mirror 'action' into 'subAction' after validation so handlers that
		// still read the legacy 'subAction' field find the value. Injecting this
		// before ValidateToolArguments would reject the 5 strict tools (subAction
		// is not a declared schema property). Mirrors HandleToolsCall :247-252.
		if (!ResolvedArgs->HasField(TEXT("subAction")) && ResolvedArgs->HasField(TEXT("action")))
		{
			FString ActionVal;
			ResolvedArgs->TryGetStringField(TEXT("action"), ActionVal);
			ResolvedArgs->SetStringField(TEXT("subAction"), ActionVal);
		}

		// Reuse the existing dispatch action resolution + subsystem queue path.
		StreamToolCall(Tool, DispatchAction, ResolvedArgs, Id, ClientSocket, SessionId, CorsOrigin, ProgressToken);
		return;
	}

	SendOneShot(FMcpJsonRpc::BuildToolResult(false,
		TEXT("operation must be search, describe, execute, or configure."),
		nullptr, TEXT("UNKNOWN_OPERATION")));
}

bool FMcpNativeTransport::HandleGatewayModePreDispatch(
	const FString& ToolName, const TSharedPtr<FJsonObject>& Arguments,
	const TSharedPtr<FJsonValue>& Id, FSocket* ClientSocket,
	const FString& SessionId, const FString& CorsOrigin,
	const TSharedPtr<FJsonValue>& ProgressToken)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	// Only the static 'unreal' tool is exposed in gateway mode.
	if (ToolName == TEXT("unreal"))
	{
		HandleGatewayCall(Arguments, Id, ClientSocket, SessionId, CorsOrigin, ProgressToken);
		return true;
	}

	// Direct canonical tool calls are rejected; reach them through 'unreal'.
	const FString ErrorBody = FMcpJsonRpc::BuildError(
		Id, FMcpJsonRpc::ErrorInvalidParams,
		TEXT("Gateway mode is enabled. Call the 'unreal' tool to search, describe, or execute capabilities."));
	SendHttpResponse(ClientSocket, 200, TEXT("application/json"), ErrorBody, {}, CorsOrigin);
	ClientSocket->Close();
	if (SocketSub) SocketSub->DestroySocket(ClientSocket);
	return true;
}

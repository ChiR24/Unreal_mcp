// McpNativeTransportGateway.cpp — route tools/call for the 'unreal' gateway tool

#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Gateway/McpNativeGatewayDefinition.h"
#include "MCP/Gateway/McpNativeGatewayCatalog.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "MCP/Gateway/McpNativeGatewayDescribe.h"
#include "MCP/Gateway/McpNativeGatewaySearch.h"

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

	// Discovery reads the generated capability store on this thread: pure data,
	// no editor API, so it never blocks the socket thread on Unreal work.
	const FMcpCapabilityStore& CapabilityStore = FMcpCapabilityStore::Get();
	auto IsToolEnabled = [this](const FString& ToolName) { return ToolManager.IsToolEnabled(ToolName); };

	auto SendDiscoveryResult = [&](const TSharedPtr<FJsonObject>& Result)
	{
		bool bOk = false;
		if (Result.IsValid()) Result->TryGetBoolField(TEXT("success"), bOk);
		const FString Msg = bOk ? TEXT("ok")
			: (Result.IsValid() ? Result->GetStringField(TEXT("message")) : TEXT("discovery failed"));
		const FString Code = bOk ? FString()
			: (Result.IsValid() ? Result->GetStringField(TEXT("errorCode")) : FString());
		SendOneShot(FMcpJsonRpc::BuildToolResult(bOk, Msg, Result, Code));
	};

	if (Operation == TEXT("search"))
	{
		FMcpDiscoveryQuery DiscoveryQuery;
		Params->TryGetStringField(TEXT("query"), DiscoveryQuery.Query);
		DiscoveryQuery.bHasDomain = Params->TryGetStringField(TEXT("domain"), DiscoveryQuery.Domain);
		DiscoveryQuery.bHasFamily = Params->TryGetStringField(TEXT("family"), DiscoveryQuery.Family);
		DiscoveryQuery.Limit = McpSearchDefaultLimit;
		if (Params->HasField(TEXT("limit")))
		{
			int32 L = 0;
			if (Params->TryGetNumberField(TEXT("limit"), L)) DiscoveryQuery.Limit = FMath::Clamp(L, 1, McpSearchMaxLimit);
		}
		if (Params->HasField(TEXT("offset")))
		{
			int32 O = 0;
			if (Params->TryGetNumberField(TEXT("offset"), O)) DiscoveryQuery.Offset = FMath::Max(0, O);
		}
		SendDiscoveryResult(McpGatewaySearchCapabilities(DiscoveryQuery, CapabilityStore, IsToolEnabled));
		return;
	}

	if (Operation == TEXT("describe"))
	{
		FMcpDiscoveryQuery DiscoveryQuery;
		Params->TryGetStringField(TEXT("tool"), DiscoveryQuery.Tool);
		DiscoveryQuery.bHasAction = Params->TryGetStringField(TEXT("action"), DiscoveryQuery.Action);
		DiscoveryQuery.bHasParam = Params->TryGetStringField(TEXT("param"), DiscoveryQuery.Param);
		Params->TryGetStringField(TEXT("query"), DiscoveryQuery.Query);
		DiscoveryQuery.Limit = McpDescribeDefaultLimit;
		if (Params->HasField(TEXT("limit")))
		{
			int32 L = 0;
			if (Params->TryGetNumberField(TEXT("limit"), L)) DiscoveryQuery.Limit = FMath::Clamp(L, 1, McpDescribeMaxLimit);
		}
		if (Params->HasField(TEXT("offset")))
		{
			int32 O = 0;
			if (Params->TryGetNumberField(TEXT("offset"), O)) DiscoveryQuery.Offset = FMath::Max(0, O);
		}
		SendDiscoveryResult(McpGatewayDescribeCapability(DiscoveryQuery, CapabilityStore, IsToolEnabled));
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
		HandleGatewayExecute(Params, Id, ClientSocket, SessionId, CorsOrigin, ProgressToken);
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

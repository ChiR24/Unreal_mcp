// McpNativeTransportGatewayExecute.cpp — canonical execute for the 'unreal' tool
//
// Split out of McpNativeTransportGateway.cpp so discovery (search/describe) and
// execute evolve independently. Session validation, dynamic tool state, the
// local-tool intercept and the subsystem queue all stay on their existing paths.

#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Execute/McpNativeGatewayValidation.h"

void FMcpNativeTransport::HandleGatewayExecute(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id,
	FSocket* ClientSocket, const FString& SessionId, const FString& CorsOrigin,
	const TSharedPtr<FJsonValue>& ProgressToken)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	const FString CorrelationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);

	auto SendReceipt = [&](const TSharedPtr<FJsonObject>& Receipt)
	{
		const bool bSucceeded = McpReceiptSucceeded(Receipt);
		FString ErrorCode;
		Receipt->TryGetStringField(TEXT("errorCode"), ErrorCode);
		const TSharedPtr<FJsonObject> ToolResult = FMcpJsonRpc::BuildToolResult(
			bSucceeded, McpReceiptMessage(Receipt), Receipt, ErrorCode);
		const FString Body = FMcpJsonRpc::BuildResponse(Id, ToolResult);
		SendHttpResponse(ClientSocket, 200, TEXT("application/json"), Body, {}, CorsOrigin);
		ClientSocket->Close();
		if (SocketSub) SocketSub->DestroySocket(ClientSocket);
	};

	// Every validation stage runs before anything is queued: an invalid request
	// can never reach editor work.
	FMcpGatewayExecutePlan Plan;
	const TSharedPtr<FJsonObject> ValidationError = ValidateAndResolveGatewayExecute(
		Params, FMcpToolRegistry::Get(), ToolManager, CorrelationId, Plan);
	if (ValidationError.IsValid())
	{
		UE_LOG(LogMcpNativeTransport, Verbose,
			TEXT("gateway execute rejected before dispatch (correlationId=%s)"), *CorrelationId);
		SendReceipt(ValidationError);
		return;
	}

	// Locally-handled tools (manage_tools) complete without queueing. This runs
	// after validation so a local tool is held to the same canonical contract.
	if (TryHandleLocalToolCall(
			Plan.ParentTool, Plan.Arguments, Id, ClientSocket, SessionId, CorsOrigin))
	{
		return;
	}

	// Handlers that still read the legacy 'subAction' field find the value here;
	// it is added after schema validation because it is not a declared property.
	if (!Plan.Arguments->HasField(TEXT("subAction")))
	{
		Plan.Arguments->SetStringField(TEXT("subAction"), Plan.LegacyAction);
	}

	StreamToolCall(
		Plan.ParentTool, Plan.DispatchAction, Plan.Arguments, Id, ClientSocket,
		SessionId, CorsOrigin, ProgressToken, Plan.CapabilityId, Plan.OutputSchema,
		CorrelationId);
}

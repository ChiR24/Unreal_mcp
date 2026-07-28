// McpNativeTransportGatewayExecute.cpp — canonical execute for the 'unreal' tool
//
// Split out of McpNativeTransportGateway.cpp so discovery (search/describe) and
// execute evolve independently. Session validation, dynamic tool state, the
// local-tool intercept and the subsystem queue all stay on their existing paths.

#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Execute/McpNativeGatewayReceipt.h"
#include "MCP/Execute/McpNativeGatewayValidation.h"
#include "MCP/Execute/McpNativeReceiptEnrichment.h"
#include "MCP/Execute/McpNativeGatewayAuthorization.h"
#include "MCP/Gateway/McpNativeGatewayGuidance.h"
#include "Core/Security/McpPrequeueGate.h"
#include "Foundation/McpIdempotencyLedger.h"
#include "HAL/PlatformTime.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void FMcpNativeTransport::HandleGatewayExecute(
	const TSharedPtr<FJsonObject>& Params, const TSharedPtr<FJsonValue>& Id,
	FSocket* ClientSocket, const FString& SessionId, const FString& CorsOrigin,
	const TSharedPtr<FJsonValue>& ProgressToken)
{
	ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	FMcpReceiptContext Context;
	Context.CorrelationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	Context.RequestId = McpCanonicalizeRequestId(Id);
	Context.StartTimeSeconds = FPlatformTime::Seconds();
	{
		const TSharedPtr<FJsonObject>* Options = nullptr;
		if (Params.IsValid() && Params->TryGetObjectField(TEXT("options"), Options) && Options)
		{
			(*Options)->TryGetStringField(TEXT("idempotencyKey"), Context.IdempotencyId);
		}
	}

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
		Params, FMcpToolRegistry::Get(), ToolManager, Context, Plan);
	if (ValidationError.IsValid())
	{
		UE_LOG(LogMcpNativeTransport, Verbose,
			TEXT("gateway execute rejected before dispatch (correlationId=%s)"), *Context.CorrelationId);
		SendReceipt(ValidationError);
		return;
	}
	Context.ExpectedRevisions = Plan.ExpectedRevisions;

	// The same pre-queue security gate the WebSocket bridge applies, so both
	// transports refuse identically, with the identical typed error, before any
	// editor work is queued. It runs before the local-tool intercept so a local
	// tool is held to the same policy as a queued one.
	{
		const FMcpCapabilityPrincipal Principal = GetSessionPrincipal(SessionId);
		FMcpPrequeueRequest AuthRequest;
		AuthRequest.Principal = &Principal;
		AuthRequest.CapabilityId = Plan.CapabilityId;
		AuthRequest.DispatchAction = Plan.DispatchAction;
		AuthRequest.Payload = Plan.Arguments;
		const TSharedPtr<FJsonObject>* ConsentField = nullptr;
		if (Params.IsValid() && Params->TryGetObjectField(TEXT("consent"), ConsentField) && ConsentField)
		{
			AuthRequest.Consent = *ConsentField;
		}

		const FMcpAuthorizationDecision Decision = McpPrequeueGate::Authorize(AuthRequest);
		if (!Decision.bAllowed)
		{
			UE_LOG(LogMcpNativeTransport, Warning,
				TEXT("gateway execute refused by policy before dispatch (%s, correlationId=%s)"),
				*Decision.ErrorCode, *Context.CorrelationId);
			// Mirrors checkScopeAuthorization/checkConsentAuthorization in
			// gateway-execute-policy.ts, which emit this same nextCall.
			TSharedPtr<FJsonObject> Guidance = MakeShared<FJsonObject>();
			Guidance->SetObjectField(TEXT("nextCall"),
				GatewayBuildNextCall(TEXT("describe"), Plan.ParentTool, Plan.LegacyAction, FString()));
			SendReceipt(McpBuildErrorReceipt(
				Plan.CapabilityId, McpAuthorizationSemanticError(Decision), Context, Guidance));
			return;
		}
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

	// Idempotency runs after the local-tool intercept (so configuration tools
	// never enter the ledger) and before dispatch (so a replay or duplicate never
	// reaches editor work). A fresh claim stashes its slot on the Context; the
	// completion funnel settles it. Engages only when a key was supplied.
	if (!Context.IdempotencyId.IsEmpty())
	{
		const FString PrincipalIdentity = GetSessionPrincipal(SessionId).Identity;
		const FString Fingerprint = McpCanonicalFingerprint(Plan.CapabilityId, Plan.Arguments);
		FString Slot;
		FString ReplayReceiptJson;
		const EMcpIdempotencyOutcome Outcome = FMcpIdempotencyLedger::Get().Begin(
			PrincipalIdentity, Plan.CapabilityId, Context.IdempotencyId, Fingerprint, Slot, ReplayReceiptJson);
		if (Outcome == EMcpIdempotencyOutcome::Replay)
		{
			TSharedPtr<FJsonObject> Recorded;
			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ReplayReceiptJson);
			if (FJsonSerializer::Deserialize(Reader, Recorded) && Recorded.IsValid())
			{
				SendReceipt(Recorded);
				return;
			}
			FMcpIdempotencyLedger::Get().Abandon(Slot);
		}
		else if (Outcome == EMcpIdempotencyOutcome::InFlight || Outcome == EMcpIdempotencyOutcome::Conflict)
		{
			const FString Detail = Outcome == EMcpIdempotencyOutcome::InFlight
				? TEXT("This idempotency key is already executing. Wait for the first call to finish and read its receipt; do not retry with the same key.")
				: TEXT("This idempotency key was already used with different parameters. Use a new idempotency key, or resend the original parameters to replay the recorded receipt.");
			SendReceipt(McpBuildErrorReceipt(
				Plan.CapabilityId, McpExecutionError(TEXT("IDEMPOTENCY_CONFLICT"), Detail, false), Context));
			return;
		}
		else if (Outcome == EMcpIdempotencyOutcome::First)
		{
			Context.IdempotencySlot = Slot;
		}
	}

	StreamToolCall(
		Plan.ParentTool, Plan.DispatchAction, Plan.Arguments, Id, ClientSocket,
		SessionId, CorsOrigin, ProgressToken, Plan.CapabilityId, Plan.OutputSchema,
		Context);
}

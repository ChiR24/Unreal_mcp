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
#include "MCP/Gateway/McpNativeGatewayExecuteReceiptBuild.h"
#include "MCP/Protocol/McpModernConsent.h"
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
	Context.QueueRequestId = FGuid::NewGuid().ToString();
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
	Context.GatewayWarnings = Plan.Warnings;
	// Who may page through the result if it is too large for one reply: this principal, capability and params.
	Context.PageOwner = MakeShared<FMcpPageOwner>();
	Context.PageOwner->Principal = GetSessionPrincipal(SessionId).Identity;
	Context.PageOwner->CapabilityId = Plan.CapabilityId;
	Context.PageOwner->Fingerprint = McpCanonicalFingerprint(Plan.CapabilityId, Plan.Arguments);
	// The continuation repeats this call's own routing, so it resolves to the same plan: sent by capability, a folded
	// old name's call lost the selector that name pins, and its fingerprint no longer matched.
	Context.PageOwner->Call = MakeShared<FJsonObject>();
	for (const TCHAR* Key : {TEXT("capability"), TEXT("tool"), TEXT("action"), TEXT("params")})
	{
		if (const TSharedPtr<FJsonValue> Value = Params->TryGetField(Key))
		{
			Context.PageOwner->Call->SetField(Key, Value);
		}
	}

	// A page cursor reads on through a result too large for one reply (McpNativeResultPages.cpp). Nothing runs again:
	// scope still holds for every page, but consent was the first call's and is not asked twice.
	const TSharedPtr<FJsonObject>* PageOptions = nullptr;
	FString PageCursor;
	if (Params->TryGetObjectField(TEXT("options"), PageOptions) && PageOptions &&
		(*PageOptions)->TryGetStringField(TEXT("pageCursor"), PageCursor))
	{
		const FMcpCapabilityPrincipal Principal = GetSessionPrincipal(SessionId);
		FMcpPrequeueRequest ScopeRequest;
		ScopeRequest.Principal = &Principal;
		ScopeRequest.CapabilityId = Plan.CapabilityId;
		ScopeRequest.DispatchAction = Plan.DispatchAction;
		ScopeRequest.Payload = Plan.Arguments;
		const FMcpAuthorizationDecision Scope =
			McpCapabilityAuthorization::CheckScope(Principal, McpPrequeueGate::ResolveDemand(ScopeRequest));
		if (!Scope.bAllowed)
		{
			SendReceipt(McpBuildErrorReceipt(Plan.CapabilityId, McpAuthorizationSemanticError(Scope), Context));
			return;
		}
		FMcpResultPage Page;
		const FString Refusal = McpNextResultPage(PageCursor, *Context.PageOwner, 100000, Page);
		if (!Refusal.IsEmpty())
		{
			TSharedPtr<FJsonObject> Guidance = MakeShared<FJsonObject>();
			Guidance->SetObjectField(TEXT("nextCall"),
				GatewayBuildNextCall(TEXT("describe"), Plan.ParentTool, Plan.LegacyAction, FString()));
			SendReceipt(McpBuildErrorReceipt(Plan.CapabilityId, McpExecutionError(Refusal, Refusal == TEXT("PAGE_CURSOR_EXPIRED")
				? TEXT("The page cursor has expired or was never issued: run the call again without pageCursor for a fresh first page.")
				: TEXT("The page cursor belongs to another call: send the same capability and params as the call that returned it."), false),
				Context, Guidance));
			return;
		}
		const TSharedPtr<FJsonObject> Receipt = McpBuildGatewayExecuteReceipt(Plan.CapabilityId, Plan.OutputSchema, Context, true,
			FString(), Page.Result, FString());
		Receipt->SetObjectField(TEXT("page"), Page.Page);
		SendReceipt(Receipt);
		return;
	}

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
		AuthRequest.RequestId = Context.QueueRequestId;
		const TSharedPtr<FJsonObject>* ConsentField = nullptr;
		if (Params.IsValid() && Params->TryGetObjectField(TEXT("consent"), ConsentField) && ConsentField)
		{
			AuthRequest.Consent = *ConsentField;
		}

		FMcpAuthorizationDecision Decision = McpPrequeueGate::Authorize(AuthRequest);
		// A 2026-07-28 call that lacks only its grant asks the user through the client (McpModernConsent); their yes
		// is the grant. The gate runs scope before consent, so nobody is asked about a call they could not make.
		McpModernConsent::FOutcome Asked;
		if (!Decision.bAllowed && Decision.ErrorCode == McpAuthorizationCodes::ConsentRequired && !AuthRequest.Consent.IsValid())
		{
			Asked = McpModernConsent::Resolve(Principal.Identity, Plan.CapabilityId,
				McpPrequeueGate::ResolveDemand(AuthRequest).ConsentMode, Plan.ParentTool + TEXT(".") + Plan.LegacyAction,
				Plan.Arguments);
			if (Asked.Ask.IsValid() || Asked.bInvalidState)
			{
				SendHttpResponse(ClientSocket, 200, TEXT("application/json"), Asked.bInvalidState
					? McpModernConsent::InvalidStateError(Id) : FMcpJsonRpc::BuildResponse(Id, Asked.Ask), {}, CorsOrigin);
				ClientSocket->Close();
				if (SocketSub) SocketSub->DestroySocket(ClientSocket);
				return;
			}
			if (Asked.Grant.IsValid())
			{
				AuthRequest.Consent = Asked.Grant;
				Decision = McpPrequeueGate::Authorize(AuthRequest);
			}
			else if (!Asked.Declined.IsEmpty())
			{
				Decision.Message = Asked.Declined;
			}
		}
		if (!Decision.bAllowed)
		{
			UE_LOG(LogMcpNativeTransport, Warning,
				TEXT("gateway execute refused by policy before dispatch (%s, correlationId=%s)"),
				*Decision.ErrorCode, *Context.CorrelationId);
			// Mirrors checkScopeAuthorization/checkConsentAuthorization in
			// gateway-execute-policy.ts, which emit this same nextCall. Not after the user said no: describe's
			// grant would run what they just refused.
			TSharedPtr<FJsonObject> Guidance = MakeShared<FJsonObject>();
			Guidance->SetObjectField(TEXT("nextCall"),
				GatewayBuildNextCall(TEXT("describe"), Plan.ParentTool, Plan.LegacyAction, FString()));
			SendReceipt(McpBuildErrorReceipt(Plan.CapabilityId, McpAuthorizationSemanticError(Decision), Context,
				Asked.Declined.IsEmpty() ? Guidance : TSharedPtr<FJsonObject>()));
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
	//
	// Stamped UNCONDITIONALLY from the server-resolved action, never merged with
	// whatever the caller sent. Domain dispatchers are split on which field they
	// read — some take `action`, some `subAction` — so honouring a caller-supplied
	// `subAction` would let it steer a dispatcher away from the capability the gate
	// authorized. RejectReservedParams already refuses both control keys in params,
	// which is why this is a no-op today; it stays unconditional so the split can
	// never reopen if that check is ever relaxed.
	Plan.Arguments->SetStringField(TEXT("subAction"), Plan.LegacyAction);

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
				// The recorded receipt describes the FIRST call, so its correlation
				// id names an exchange this client never made. Sent unchanged, a
				// client joining on correlationId saw a mismatch it could not tell
				// from a bug. The envelope names THIS call, `replayed` says why the
				// body is the earlier one, and `replayedFrom` keeps the original
				// joinable. Mirrors markReplayed in gateway-execute-idempotency.ts.
				FString RecordedCorrelation;
				if (Recorded->TryGetStringField(TEXT("correlationId"), RecordedCorrelation))
				{
					Recorded->SetStringField(TEXT("replayedFrom"), RecordedCorrelation);
				}
				Recorded->SetBoolField(TEXT("replayed"), true);
				Recorded->SetStringField(TEXT("correlationId"), Context.CorrelationId);
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

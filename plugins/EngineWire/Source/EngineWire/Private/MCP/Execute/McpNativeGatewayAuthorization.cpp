#include "MCP/Execute/McpNativeGatewayAuthorization.h"

#include "Core/Security/McpPrequeueGate.h"
#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "EngineWireSettings.h"

#include <atomic>

namespace
{
// Whether the running listener is bound to loopback only (set by Start from its real bind). Read from the settings,
// switching bAllowNonLoopback and bRequireCapabilityToken off while a LAN listener stayed up handed remote, token-free
// requests the loopback Admin principal. False (fail closed) until Start binds.
std::atomic<bool> GListenerLoopback{false};
}

void McpSetNativeListenerLoopback(bool bLoopback)
{
	GListenerLoopback.store(bLoopback);
}

FMcpCapabilityPrincipal McpResolveNativePrincipal(const FString& PresentedToken)
{
	const UEngineWireSettings* Settings = GetDefault<UEngineWireSettings>();

	FMcpPrincipalResolveRequest Request;
	Request.PresentedToken = PresentedToken;
	Request.bRequireToken = Settings->bRequireCapabilityToken;
	Request.bIsLoopback = GListenerLoopback.load();

	return McpCapabilityPrincipal::Resolve(Request, *Settings);
}

bool McpIsNativePrincipalSwap(
	const FMcpCapabilityPrincipal& Bound, const FMcpCapabilityPrincipal& Presented)
{
	// Identity is the stable, non-secret principal name. Comparing identities
	// rather than tokens detects a swap without ever storing a token on a session.
	return Bound.Identity != Presented.Identity ||
	       Bound.bAuthenticated != Presented.bAuthenticated;
}

FMcpSemanticError McpAuthorizationSemanticError(const FMcpAuthorizationDecision& Decision)
{
	FMcpSemanticError Error;
	Error.Code = Decision.ErrorCode;
	Error.GatewayCode = Decision.ErrorCode;
	Error.Message = Decision.Message;
	Error.RequiredScope = Decision.RequiredScope;
	Error.GrantedScopes = Decision.GrantedScopes;
	Error.ConsentScope = Decision.ConsentScope;

	static const TMap<FString, const TCHAR*> KindByCode = {
		{ McpAuthorizationCodes::ScopeNotGranted, TEXT("authorization") },
		{ McpAuthorizationCodes::ConsentRequired, TEXT("consent") },
		{ McpAuthorizationCodes::ConsentNotIssued, TEXT("consent") },
		{ McpAuthorizationCodes::ProjectNotPermitted, TEXT("project") },
		{ McpAuthorizationCodes::PathNotPermitted, TEXT("pathPolicy") },
		{ McpAuthorizationCodes::CommandBlocked, TEXT("command") },
		{ McpAuthorizationCodes::QuotaExceeded, TEXT("quota") },
	};
	const TCHAR* const* Kind = KindByCode.Find(Decision.ErrorCode);
	Error.Kind = Kind ? *Kind : TEXT("validation");
	if (Decision.ErrorCode == McpAuthorizationCodes::QuotaExceeded)
	{
		Error.bHasRetryable = true;
		Error.bRetryable = Decision.bRetryable;
	}
	return Error;
}

FString McpAuthorizePrimitiveRead(
	const FMcpCapabilityPrincipal& Principal, const FString& ResourceUri,
	const TSharedPtr<FJsonValue>& Id)
{
	FMcpPrequeueRequest Request;
	Request.Principal = &Principal;
	// Discovery reads project data but runs no editor work, so it charges the
	// principal's request budget and never its tool-call budget.
	Request.bIsToolCall = false;

	// Only the addressable templates carry a content path; the fixed resources
	// (ue://project, ue://editor, ...) address no path at all.
	static const TCHAR* const PathPrefixes[] = { TEXT("ue://object/"), TEXT("ue://asset/") };
	for (const TCHAR* const Prefix : PathPrefixes)
	{
		const FString PrefixText(Prefix);
		if (!ResourceUri.StartsWith(PrefixText, ESearchCase::IgnoreCase))
		{
			continue;
		}
		FString Addressed = ResourceUri.RightChop(PrefixText.Len());
		if (!Addressed.IsEmpty() && !Addressed.StartsWith(TEXT("/")))
		{
			Addressed = TEXT("/") + Addressed;
		}
		Request.Payload = MakeShared<FJsonObject>();
		Request.Payload->SetStringField(TEXT("path"), Addressed);
		break;
	}

	const FMcpAuthorizationDecision Decision = McpPrequeueGate::AuthorizeRead(Request);
	if (Decision.bAllowed)
	{
		return FString();
	}

	UE_LOG(LogMcpNativeTransport, Warning,
		TEXT("Refused an MCP primitive before dispatch (%s)."), *Decision.ErrorCode);
	auto Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("code"), Decision.ErrorCode);
	return FMcpJsonRpc::BuildError(
		Id, FMcpJsonRpc::ErrorInvalidRequest, Decision.Message, Data);
}

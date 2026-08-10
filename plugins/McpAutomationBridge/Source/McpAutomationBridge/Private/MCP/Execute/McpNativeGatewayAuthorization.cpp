#include "MCP/Execute/McpNativeGatewayAuthorization.h"

#include "Core/Security/McpPrequeueGate.h"
#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "McpAutomationBridgeSettings.h"

FMcpCapabilityPrincipal McpResolveNativePrincipal(const FString& PresentedToken)
{
	const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();

	FMcpPrincipalResolveRequest Request;
	Request.PresentedToken = PresentedToken;
	Request.bRequireToken = Settings->bRequireCapabilityToken;
	// The native transport refuses to bind non-loopback unless a token is also
	// required, so a no-token connection only ever reaches here on loopback.
	Request.bIsLoopback = !Settings->bAllowNonLoopback;

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

	if (Decision.ErrorCode == McpAuthorizationCodes::ScopeNotGranted)
	{
		Error.Kind = TEXT("authorization");
	}
	else if (Decision.ErrorCode == McpAuthorizationCodes::ConsentRequired)
	{
		Error.Kind = TEXT("consent");
	}
	else if (Decision.ErrorCode == McpAuthorizationCodes::ProjectNotPermitted)
	{
		Error.Kind = TEXT("project");
	}
	else if (Decision.ErrorCode == McpAuthorizationCodes::PathNotPermitted)
	{
		Error.Kind = TEXT("pathPolicy");
	}
	else if (Decision.ErrorCode == McpAuthorizationCodes::CommandBlocked)
	{
		Error.Kind = TEXT("command");
	}
	else if (Decision.ErrorCode == McpAuthorizationCodes::QuotaExceeded)
	{
		Error.Kind = TEXT("quota");
		Error.bHasRetryable = true;
		Error.bRetryable = Decision.bRetryable;
	}
	else
	{
		Error.Kind = TEXT("validation");
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

// Session-principal state lives on FMcpNativeTransport but is defined here so the
// Transport/ folder stays at its 25-file / 250-line ceilings.

void FMcpNativeTransport::BindSessionPrincipal(
	const FString& SessionId, const FString& PresentedToken)
{
	if (SessionId.IsEmpty())
	{
		return;
	}
	const FMcpCapabilityPrincipal Principal = McpResolveNativePrincipal(PresentedToken);
	FScopeLock Lock(&SessionMutex);
	SessionPrincipals.Add(SessionId, Principal);
}

FMcpCapabilityPrincipal FMcpNativeTransport::GetSessionPrincipal(const FString& SessionId)
{
	FScopeLock Lock(&SessionMutex);
	if (const FMcpCapabilityPrincipal* Found = SessionPrincipals.Find(SessionId))
	{
		return *Found;
	}
	return FMcpCapabilityPrincipal{};
}

bool FMcpNativeTransport::VerifySessionPrincipal(
	const FString& SessionId, const FString& PresentedToken)
{
	FMcpCapabilityPrincipal Bound;
	{
		FScopeLock Lock(&SessionMutex);
		const FMcpCapabilityPrincipal* Found = SessionPrincipals.Find(SessionId);
		// A session with no bound principal predates binding or was already torn
		// down; session validation owns that case, so this gate stays out of it.
		if (!Found)
		{
			return true;
		}
		Bound = *Found;
	}
	return !McpIsNativePrincipalSwap(Bound, McpResolveNativePrincipal(PresentedToken));
}

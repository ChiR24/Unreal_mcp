// The principal each native session acts as: bound at initialize from the token presented, checked on every later
// request so a session never changes hands, and refreshed so a narrowed profile takes effect at once.
#include "MCP/Transport/McpNativeTransportPrivate.h"
#include "MCP/Execute/McpNativeGatewayAuthorization.h"

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
	const FMcpCapabilityPrincipal Presented = McpResolveNativePrincipal(PresentedToken);
	if (McpIsNativePrincipalSwap(Bound, Presented))
	{
		return false;
	}
	// Same identity, so the same caller: take its authority as it is NOW. The session kept the scopes, prefixes and
	// quotas of the moment it was created, so a profile narrowed in the settings kept its old reach on every session
	// opened before the edit. Quota state is keyed by identity, so nothing resets.
	FScopeLock Lock(&SessionMutex);
	if (SessionPrincipals.Contains(SessionId))
	{
		SessionPrincipals.Add(SessionId, Presented);
	}
	return true;
}

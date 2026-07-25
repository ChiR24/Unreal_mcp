#pragma once

#include "CoreMinimal.h"

#include "McpCapabilityScopes.h"

class UMcpAutomationBridgeSettings;

// A resolved, non-secret security principal. Identity is safe to log/echo
// ("loopback", "legacy", or "scoped:<normalized-profile>"); the presented token
// is never stored here. The plugin is the sole authority: this principal is bound
// to a WebSocket socket or a native /mcp session and re-consulted on every request.
struct FMcpCapabilityPrincipal
{
	FString Identity;
	TArray<EMcpCapabilityScope> Scopes;
	TArray<FString> AllowedPathPrefixes;
	TArray<FString> AllowedProjects;
	int32 MaxRequestsPerMinute = 0;
	int32 MaxToolCallsPerMinute = 0;
	bool bAuthenticated = false;
	bool bDeprecated = false;

	// Exact-set membership with an Admin wildcard. NOT rank-based: holding Write
	// never implies Read or Destructive. An empty scope set authorizes nothing.
	bool IsScopeAuthorized(EMcpCapabilityScope Required) const;

	bool IsPathRestricted() const { return AllowedPathPrefixes.Num() > 0; }
	bool IsProjectRestricted() const { return AllowedProjects.Num() > 0; }
};

// Inputs for one principal resolution, grouped so the resolver stays within the
// argument budget and the presented token is passed by const-ref, never copied.
struct FMcpPrincipalResolveRequest
{
	FString PresentedToken;
	bool bIsLoopback = false;
	bool bRequireToken = false;
};

namespace McpCapabilityPrincipal
{
	// Resolve the principal for a presented token. Every candidate (each valid
	// scoped token, then the legacy token) is compared in constant time with no
	// early break, so timing never reveals which token matched. A scoped token that
	// collides with the legacy token wins (narrower). No-token loopback binds admin;
	// a required-but-unmatched token yields an unauthenticated principal.
	FMcpCapabilityPrincipal Resolve(
		const FMcpPrincipalResolveRequest& Request,
		const UMcpAutomationBridgeSettings& Settings);

	// Lower-case wire name for a scope, used in authority descriptors and errors.
	FString ScopeToString(EMcpCapabilityScope Scope);
}

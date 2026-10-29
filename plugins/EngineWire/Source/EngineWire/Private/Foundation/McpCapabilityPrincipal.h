#pragma once

#include "CoreMinimal.h"

#include "McpCapabilityScopes.h"

class UEngineWireSettings;

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
	// McpCapabilityPrincipal::AuthorityRevision() when Resolve made this; a holder compares it to see staleness.
	uint32 AuthorityRevision = 0;
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
		const UEngineWireSettings& Settings);

	// Lower-case wire name for a scope, used in authority descriptors and errors.
	FString ScopeToString(EMcpCapabilityScope Scope);

	// Moves whenever the token candidates are rebuilt: a settings edit, or a token file replaced on disk. A principal
	// a connection resolved under an older revision may carry authority its token no longer has.
	uint32 AuthorityRevision(const UEngineWireSettings& Settings);

	// The principal of the request whose handler runs on the game thread now; null between requests, off the game
	// thread, and for a request the gate never saw. ProcessAutomationRequest sets it around each dispatch.
	const FMcpCapabilityPrincipal* ActiveRequestPrincipal();

	// Makes Principal the active one until destroyed; the previous one comes back, so in-process steps nest.
	struct FActiveRequestScope
	{
		explicit FActiveRequestScope(const FMcpCapabilityPrincipal* Principal);
		~FActiveRequestScope();
		const FMcpCapabilityPrincipal* Previous;
	};

	// False when the active principal is path-restricted and PackageName lies under none of its prefixes. The gate
	// checks the paths a request names; a handler can still reach others (a default folder, a label, a reference,
	// a dependent asset), so the shared save, delete and property-walk sinks ask this before they act.
	bool ActiveRequestPermitsPath(const FString& PackageName);

	// The same for an object: an actor, or a component of one, counts as its level's map package (a World Partition
	// actor's own package is under /Game/__ExternalActors__); anything else as its outermost package. True for null.
	bool ActiveRequestPermitsObject(const UObject* Object);

	// Whether the active principal holds Scope (true with none active). For an action that does more than its
	// record's scope only in some calls, inside a folded family where policy.raisedBy would raise every sibling too.
	bool ActiveRequestHoldsScope(EMcpCapabilityScope Scope);
}

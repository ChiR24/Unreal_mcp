#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#include "Foundation/McpCapabilityPrincipal.h"

// Task 40 pure authorization predicates shared by BOTH transports (the WebSocket
// bridge and native /mcp). Everything here is side-effect free and depends only
// on Foundation, so the same decision is reproducible in an automation test with
// no editor, socket or session. Composition with the capability catalogue, the
// console-command policy and the quota ledger happens one layer up in
// Core/Security/McpPrequeueGate.h; this header never reaches into Domains or MCP.

// What a capability demands of its caller. The defaults are FAIL-CLOSED: an
// action that is not in the canonical catalogue demands Admin, so a raw
// WebSocket client naming an unknown action is refused unless it holds Admin.
struct FMcpCapabilityDemand
{
	EMcpCapabilityScope RequiredScope = EMcpCapabilityScope::Admin;
	// "none" | "explicit" | "elevated" — mirrors CONSENT_MODES in TypeScript.
	FString ConsentMode = TEXT("none");
	FString CapabilityId;

	// True when the resolved capability DECLARES a path-bearing parameter, so a
	// caller who omits it gets a SERVER-SIDE default folder the payload never
	// mentions. Defaults true: an unresolved capability fails closed.
	bool bDeclaresPathParameter = true;
};

// What the caller presented. Consent arrives as an automation_request envelope
// sibling (never a handler param) and is re-validated here. The plugin never
// infers consent from loopback, a prior call, idempotency or preview.
struct FMcpAuthorizationGrant
{
	FString ConsentCapability;
	FString ConsentAcknowledge;
	bool bConsentPresent = false;
};

// Stable machine-readable refusal codes. These are the SAME strings the
// TypeScript semantic error algebra emits, so typed parity holds by construction.
namespace McpAuthorizationCodes
{
extern const TCHAR* const ScopeNotGranted;
extern const TCHAR* const ConsentRequired;
extern const TCHAR* const PathNotPermitted;
extern const TCHAR* const ProjectNotPermitted;
extern const TCHAR* const QuotaExceeded;
extern const TCHAR* const CommandBlocked;
} // namespace McpAuthorizationCodes

struct FMcpAuthorizationDecision
{
	bool bAllowed = true;
	FString ErrorCode;
	FString Message;

	// Typed-error payload mirroring the TypeScript discriminated union exactly:
	// authorization/SCOPE_NOT_GRANTED carries requiredScope + grantedScopes,
	// consent/CONSENT_REQUIRED carries scope, quota/QUOTA_EXCEEDED carries
	// retryable. Carried on the decision so BOTH transports build the identical
	// typed error from one source instead of each re-deriving it.
	FString RequiredScope;
	TArray<FString> GrantedScopes;
	FString ConsentScope;
	bool bRetryable = false;

	static FMcpAuthorizationDecision Allow();
	static FMcpAuthorizationDecision Deny(const TCHAR* Code, const FString& InMessage);
};

// One bounded payload scan. bTruncated is the honest answer to "did the scan
// reach every value?" — when it is true the collected list proves NOTHING about
// what the scan never visited, so a hostile client cannot pad a payload past the
// node budget or nest below the depth limit and have its real path go uncollected.
struct FMcpPayloadPathScan
{
	TArray<FString> Paths;
	bool bTruncated = false;
};

namespace McpCapabilityAuthorization
{
// Exact-set scope membership with an Admin wildcard, refused with the caller-safe
// required/granted summary. Never emits the presented token.
FMcpAuthorizationDecision CheckScope(
	const FMcpCapabilityPrincipal& Principal, const FMcpCapabilityDemand& Demand);

// "none" passes without a grant; "explicit" accepts explicit|elevated; "elevated"
// accepts only elevated. A grant is honoured only when it names THIS capability,
// so consent for one capability can never authorize another.
FMcpAuthorizationDecision CheckConsent(
	const FMcpCapabilityDemand& Demand, const FMcpAuthorizationGrant& Grant);

// Boundary-aware containment: the prefix "/Game/Team" permits "/Game/Team" and
// "/Game/Team/Sub" but never "/Game/TeamOther".
bool IsPathWithinPrefix(const FString& Path, const FString& Prefix);

// An unrestricted principal passes. A restricted principal must have EVERY
// candidate path inside at least one allowed prefix; a request that carries no
// path passes.
FMcpAuthorizationDecision CheckPaths(
	const FMcpCapabilityPrincipal& Principal, const TArray<FString>& Paths);

FMcpAuthorizationDecision CheckProject(
	const FMcpCapabilityPrincipal& Principal, const FString& ProjectName);

// Every UE-rooted path anywhere in the payload, CANONICALIZED first (separator,
// `/Content` alias, bare-relative rooting) so the gate sees the same string the
// handler will resolve. Values are scanned rather than read from an allowlist of
// key names, so an unusual key cannot smuggle a path past the gate. Recursion
// depth and visited-node count are bounded.
//
// bStrict is set for a path-RESTRICTED principal only. It widens collection to
// bare-relative values under path parameters and to values whose canonical form
// was refused, so such a request fails closed instead of collecting nothing and
// being admitted. An unrestricted principal is unaffected.
FMcpPayloadPathScan CollectPayloadPaths(const TSharedPtr<FJsonObject>& Payload, bool bStrict = false);

// True for a parameter name a handler roots at `/Game` even with no separator,
// so `packagePath: "Secret"` becomes `/Game/Secret`. Shared by payload
// collection and by capability-record inspection so "is this a path parameter?"
// has exactly ONE answer on both sides of the gate.
bool IsPathParameterKey(const FString& Key);

// The rule that closes the escapes value scanning structurally CANNOT see: a
// folder/name join where only the folder was checked, an optional path
// parameter left out so a server-side default applies, and a bare-relative
// value under a key no allowlist names. For a path-RESTRICTED principal running
// a project-MUTATING capability that DECLARES a path parameter, the request must
// carry at least one provable in-prefix target — and the scan that looked for it
// must have run to completion.
FMcpAuthorizationDecision CheckPathCoverage(
	const FMcpCapabilityPrincipal& Principal,
	const FMcpCapabilityDemand& Demand,
	const FMcpPayloadPathScan& Scan);
} // namespace McpCapabilityAuthorization

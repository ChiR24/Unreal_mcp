#include "Foundation/McpCapabilityPrincipal.h"
#include "Foundation/BridgeHelpers/Security/McpAutomationBridgeHelpersCapabilityToken.h"

#include "McpAutomationBridgeLog.h"
#include "McpAutomationBridgeSettings.h"
#include "Foundation/McpSecureTokenCompare.h"

bool FMcpCapabilityPrincipal::IsScopeAuthorized(EMcpCapabilityScope Required) const
{
	return Scopes.Contains(EMcpCapabilityScope::Admin) || Scopes.Contains(Required);
}

namespace McpCapabilityPrincipal
{
FString ScopeToString(EMcpCapabilityScope Scope)
{
	switch (Scope)
	{
	case EMcpCapabilityScope::Read:
		return TEXT("read");
	case EMcpCapabilityScope::Write:
		return TEXT("write");
	case EMcpCapabilityScope::Destructive:
		return TEXT("destructive");
	case EMcpCapabilityScope::Admin:
		return TEXT("admin");
	}
	return TEXT("unknown");
}

namespace
{
struct FCandidate
{
	FString Token;
	FMcpCapabilityPrincipal Principal;
};

// Build the clean candidate table from settings. Scoped tokens are added first so
// a scoped token that collides with the legacy token wins (narrower); the legacy
// token then maps explicitly to Admin (deprecated) only if still unclaimed. A
// scoped entry with an empty token/profile, an Admin scope, or a duplicate
// profile/token is invalid and ignored with a token-free warning.
TArray<FCandidate> BuildCandidates(const UMcpAutomationBridgeSettings& Settings)
{
	TArray<FCandidate> Candidates;
	TSet<FString> SeenProfiles;
	TSet<FString> SeenTokens;

	for (const FMcpScopedCapabilityToken& Entry : Settings.ScopedCapabilityTokens)
	{
		const FString Profile = Entry.Profile.TrimStartAndEnd().ToLower();
		if (Entry.Token.IsEmpty() || Profile.IsEmpty())
		{
			UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
				TEXT("Ignoring scoped capability token with an empty token or profile."));
			continue;
		}
		if (Entry.Scopes.Contains(EMcpCapabilityScope::Admin))
		{
			UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
				TEXT("Ignoring scoped capability token '%s': scoped tokens may not grant admin."), *Profile);
			continue;
		}
		if (SeenProfiles.Contains(Profile) || SeenTokens.Contains(Entry.Token))
		{
			UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
				TEXT("Ignoring scoped capability token '%s': duplicate profile or token."), *Profile);
			continue;
		}
		SeenProfiles.Add(Profile);
		SeenTokens.Add(Entry.Token);

		FMcpCapabilityPrincipal Scoped;
		Scoped.Identity = FString::Printf(TEXT("scoped:%s"), *Profile);
		Scoped.Scopes = Entry.Scopes;
		Scoped.AllowedPathPrefixes = Entry.AllowedPathPrefixes;
		Scoped.AllowedProjects = Entry.AllowedProjects;
		Scoped.MaxRequestsPerMinute = Entry.MaxRequestsPerMinute;
		Scoped.MaxToolCallsPerMinute = Entry.MaxToolCallsPerMinute;
		Scoped.bAuthenticated = true;
		Candidates.Add({ Entry.Token, MoveTemp(Scoped) });
	}

	if (!Settings.CapabilityToken.IsEmpty() && !SeenTokens.Contains(Settings.CapabilityToken))
	{
		FMcpCapabilityPrincipal Legacy;
		Legacy.Identity = TEXT("legacy");
		Legacy.Scopes = { EMcpCapabilityScope::Admin };
		Legacy.bAuthenticated = true;
		Legacy.bDeprecated = true;
		Candidates.Add({ Settings.CapabilityToken, MoveTemp(Legacy) });
		// Record it, exactly as the scoped loop above records its own. Without
		// this the store step below re-adds the SAME token as 'legacy-store',
		// and because Resolve scans every candidate without breaking (constant
		// time, deliberately) the later duplicate wins. Any operator who set an
		// explicit CapabilityToken was therefore attributed to 'legacy-store'
		// in audit and telemetry. Same scopes, so no privilege change -- a
		// wrong identity string, which is the part that has to be right.
		SeenTokens.Add(Settings.CapabilityToken);
	}

	// Also add the effective token from the store (auto-generated + persisted if
	// needed) as a legacy candidate so that Resolve can match against it.
	// This ensures the native /mcp transport uses the same effective token as the
	// store-generated token, not just Settings->CapabilityToken.
	// NOTE: This is idempotent. GetCandidateTable caches this table per settings
	// revision, so the token file is read at most once per revision — not on
	// every request. The first run may auto-generate and persist the token, and
	// the store re-reads the file so concurrent first-run writers converge.
	const FString EffectiveToken = McpCapabilityTokenStore::ResolveEffectiveToken(&Settings);
	if (!EffectiveToken.IsEmpty() && !SeenTokens.Contains(EffectiveToken))
	{
		FMcpCapabilityPrincipal LegacyStore;
		LegacyStore.Identity = TEXT("legacy-store");
		LegacyStore.Scopes = { EMcpCapabilityScope::Admin };
		LegacyStore.bAuthenticated = true;
		LegacyStore.bDeprecated = true;
		Candidates.Add({ EffectiveToken, MoveTemp(LegacyStore) });
	}

	return Candidates;
}

using FCandidateTable = TArray<FCandidate>;
using FCandidateTableRef = TSharedPtr<const FCandidateTable, ESPMode::ThreadSafe>;

// Built once per settings revision instead of once per request. Rebuilding per
// request re-ran validation — and so re-logged every warning for a malformed
// entry — on every single call, forever, and allocated the whole table each time.
// A shared immutable snapshot is handed out, so a concurrent rebuild can never
// mutate a table another thread is scanning.
FCandidateTableRef GetCandidateTable(const UMcpAutomationBridgeSettings& Settings)
{
	static FCriticalSection CacheMutex;
	static FCandidateTableRef Cached;
	static int32 CachedGeneration = -1;
	// Weak, not raw. A raw pointer identifies an address, not an object: once a
	// settings object is collected, the next NewObject can land on that address
	// and — because GetEditGeneration() counts editor property edits and plain
	// C++ assignment never bumps it — both halves of the key would match while
	// the table described a different object entirely. A weak pointer carries a
	// serial number, so a recycled address reads as a miss and rebuilds.
	static TWeakObjectPtr<const UMcpAutomationBridgeSettings> CachedSettings;

	// Cheap, non-secret staleness key: the settings revision counter and the
	// object identity. A token value is never part of it. Do NOT fold in entry
	// counts as a packed int — Num()*1000 + Len() aliases, and the revision
	// counter already changes on every edit.
	//
	// ROTATION GAP: GetEditGeneration() counts editor property edits only, so
	// rotating the on-disk token file (Saved/MCP/capability-token) to revoke a
	// leaked credential does NOT invalidate this cache — the stale table (with
	// the old token's principal mapping) persists until an editor property edit
	// bumps the generation or the editor restarts. If immediate revocation is
	// required, call GetCandidateTable after deleting/rewriting the token file
	// and force a rebuild by temporarily toggling a settings property, or clear
	// the cache by restarting the editor. A future fix could add the token
	// file's modification time to the staleness key.
	const int32 Generation = UMcpAutomationBridgeSettings::GetEditGeneration();

	FScopeLock Lock(&CacheMutex);
	if (!Cached.IsValid() || CachedGeneration != Generation || CachedSettings.Get() != &Settings)
	{
		Cached = MakeShared<const FCandidateTable, ESPMode::ThreadSafe>(BuildCandidates(Settings));
		CachedGeneration = Generation;
		CachedSettings = &Settings;
	}
	return Cached;
}
} // namespace

FMcpCapabilityPrincipal Resolve(
	const FMcpPrincipalResolveRequest& Request,
	const UMcpAutomationBridgeSettings& Settings)
{
	const FCandidateTableRef TableRef = GetCandidateTable(Settings);
	const FCandidateTable& Candidates = *TableRef;

	// Constant-time scan: compare the presented token against EVERY candidate with
	// no early break, so comparison time never reveals which (or how much of a)
	// token matched. Candidate tokens are non-empty, so an empty presented token
	// matches nothing.
	int32 MatchIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		if (McpConstantTimeTokenEquals(Request.PresentedToken, Candidates[Index].Token))
		{
			MatchIndex = Index;
		}
	}

	if (MatchIndex != INDEX_NONE)
	{
		return Candidates[MatchIndex].Principal;
	}

	// No candidate matched. Loopback with no token presented and none required
	// binds admin (preserves the loopback default); every other case is unauthenticated.
	FMcpCapabilityPrincipal Principal;
	if (!Request.bRequireToken && Request.bIsLoopback && Request.PresentedToken.IsEmpty())
	{
		Principal.Identity = TEXT("loopback");
		Principal.Scopes = { EMcpCapabilityScope::Admin };
		Principal.bAuthenticated = true;
	}
	return Principal;
}
} // namespace McpCapabilityPrincipal

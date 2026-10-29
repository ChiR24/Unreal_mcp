#include "Foundation/McpCapabilityPrincipal.h"
#include "Foundation/BridgeHelpers/Security/McpCapToken.h"

#include "EngineWireLog.h"
#include "EngineWireSettings.h"
#include "Components/ActorComponent.h"
#include "Engine/Level.h"
#include "Foundation/McpCapabilityAuthorization.h"
#include "Foundation/McpSecureTokenCompare.h"
#include "GameFramework/Actor.h"

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
TArray<FCandidate> BuildCandidates(const UEngineWireSettings& Settings, const FString& StoreToken)
{
	TArray<FCandidate> Candidates;
	TSet<FString> SeenProfiles;
	TSet<FString> SeenTokens;

	for (const FMcpScopedCapabilityToken& Entry : Settings.ScopedCapabilityTokens)
	{
		const FString Profile = Entry.Profile.TrimStartAndEnd().ToLower();
		if (Entry.Token.IsEmpty() || Profile.IsEmpty())
		{
			UE_LOG(LogEngineWireSubsystem, Warning,
				TEXT("Ignoring scoped capability token with an empty token or profile."));
			continue;
		}
		if (Entry.Scopes.Contains(EMcpCapabilityScope::Admin))
		{
			UE_LOG(LogEngineWireSubsystem, Warning,
				TEXT("Ignoring scoped capability token '%s': scoped tokens may not grant admin."), *Profile);
			continue;
		}
		if (SeenProfiles.Contains(Profile) || SeenTokens.Contains(Entry.Token))
		{
			UE_LOG(LogEngineWireSubsystem, Warning,
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

	// Both legacy tokens are admin and deprecated; only the identity differs. Each
	// is recorded in SeenTokens like a scoped one, because Resolve scans every
	// candidate without breaking (constant time) and a later duplicate would win:
	// an explicit CapabilityToken used to be re-added, and so audited, as
	// 'legacy-store'. Same scopes either way; the identity is what must be right.
	auto AddLegacy = [&Candidates, &SeenTokens](const FString& Token, const TCHAR* Identity)
	{
		if (Token.IsEmpty() || SeenTokens.Contains(Token))
		{
			return;
		}
		FMcpCapabilityPrincipal Legacy;
		Legacy.Identity = Identity;
		Legacy.Scopes = { EMcpCapabilityScope::Admin };
		Legacy.bAuthenticated = true;
		Legacy.bDeprecated = true;
		Candidates.Add({ Token, MoveTemp(Legacy) });
		SeenTokens.Add(Token);
	};
	AddLegacy(Settings.CapabilityToken, TEXT("legacy"));
	// The store's effective token (explicit, persisted, or auto-generated once) so
	// native /mcp matches the same token the bridge uses.
	AddLegacy(StoreToken, TEXT("legacy-store"));

	return Candidates;
}

using FCandidateTable = TArray<FCandidate>;
using FCandidateTableRef = TSharedPtr<const FCandidateTable, ESPMode::ThreadSafe>;

// Built once per settings revision instead of once per request. Rebuilding per
// request re-ran validation — and so re-logged every warning for a malformed
// entry — on every single call, forever, and allocated the whole table each time.
// A shared immutable snapshot is handed out, so a concurrent rebuild can never
// mutate a table another thread is scanning. OutRevision counts the rebuilds, read
// under the same lock as the table it numbers.
FCandidateTableRef GetCandidateTable(const UEngineWireSettings& Settings, uint32& OutRevision)
{
	static FCriticalSection CacheMutex;
	static FCandidateTableRef Cached;
	static int32 CachedGeneration = -1;
	static FString CachedStoreToken;
	static uint32 Revision = 0;
	// Weak, not raw. A raw pointer identifies an address, not an object: once a
	// settings object is collected, the next NewObject can land on that address
	// and — because GetEditGeneration() counts editor property edits and plain
	// C++ assignment never bumps it — both halves of the key would match while
	// the table described a different object entirely. A weak pointer carries a
	// serial number, so a recycled address reads as a miss and rebuilds.
	static TWeakObjectPtr<const UEngineWireSettings> CachedSettings;

	// Staleness key: the settings revision counter, the object identity, and the
	// store's effective token. GetEditGeneration() counts editor property edits
	// only, so a token file rotated on disk to revoke a leaked credential kept the
	// old token authenticating until the next edit or restart. The file is read
	// here on every call (native /mcp already reads it per request); compared with
	// the copy the table was built from, it never leaves this function. Do NOT
	// fold in entry counts as a packed int: Num()*1000 + Len() aliases.
	const int32 Generation = UEngineWireSettings::GetEditGeneration();
	const FString StoreToken = McpCapabilityTokenStore::ResolveEffectiveToken(&Settings);

	FScopeLock Lock(&CacheMutex);
	if (!Cached.IsValid() || CachedGeneration != Generation || CachedSettings.Get() != &Settings ||
		!CachedStoreToken.Equals(StoreToken, ESearchCase::CaseSensitive))
	{
		Cached = MakeShared<const FCandidateTable, ESPMode::ThreadSafe>(BuildCandidates(Settings, StoreToken));
		CachedGeneration = Generation;
		CachedSettings = &Settings;
		CachedStoreToken = StoreToken;
		++Revision;
	}
	OutRevision = Revision;
	return Cached;
}

// The running request's principal (game thread only; see ActiveRequestPrincipal).
const FMcpCapabilityPrincipal* GActiveRequestPrincipal = nullptr;
} // namespace

uint32 AuthorityRevision(const UEngineWireSettings& Settings)
{
	uint32 Revision = 0;
	GetCandidateTable(Settings, Revision);
	return Revision;
}

const FMcpCapabilityPrincipal* ActiveRequestPrincipal()
{
	return IsInGameThread() ? GActiveRequestPrincipal : nullptr;
}

FActiveRequestScope::FActiveRequestScope(const FMcpCapabilityPrincipal* Principal)
	: Previous(GActiveRequestPrincipal)
{
	check(IsInGameThread());
	GActiveRequestPrincipal = Principal;
}

FActiveRequestScope::~FActiveRequestScope()
{
	GActiveRequestPrincipal = Previous;
}

bool ActiveRequestPermitsPath(const FString& PackageName)
{
	const FMcpCapabilityPrincipal* Principal = ActiveRequestPrincipal();
	return !Principal || !Principal->IsPathRestricted() ||
		Principal->AllowedPathPrefixes.ContainsByPredicate([&PackageName](const FString& Prefix)
		{
			return McpCapabilityAuthorization::IsPathWithinPrefix(PackageName, Prefix);
		});
}

bool ActiveRequestPermitsObject(const UObject* Object)
{
	const FMcpCapabilityPrincipal* Principal = ActiveRequestPrincipal();
	if (!Object || !Principal || !Principal->IsPathRestricted())
	{
		return true;
	}
	const UActorComponent* Component = Cast<UActorComponent>(Object);
	const AActor* Actor = Component ? Component->GetOwner() : Cast<AActor>(Object);
	const UObject* Packaged = Actor && Actor->GetLevel() ? static_cast<const UObject*>(Actor->GetLevel()) : Object;
	return ActiveRequestPermitsPath(Packaged->GetOutermost()->GetName());
}

bool ActiveRequestHoldsScope(EMcpCapabilityScope Scope)
{
	const FMcpCapabilityPrincipal* Principal = ActiveRequestPrincipal();
	return !Principal || Principal->IsScopeAuthorized(Scope);
}

FMcpCapabilityPrincipal Resolve(
	const FMcpPrincipalResolveRequest& Request,
	const UEngineWireSettings& Settings)
{
	uint32 Revision = 0;
	const FCandidateTableRef TableRef = GetCandidateTable(Settings, Revision);
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

	FMcpCapabilityPrincipal Principal;
	if (MatchIndex != INDEX_NONE)
	{
		Principal = Candidates[MatchIndex].Principal;
	}
	// No candidate matched. Loopback with no token presented and none required
	// binds admin (preserves the loopback default); every other case is unauthenticated.
	else if (!Request.bRequireToken && Request.bIsLoopback && Request.PresentedToken.IsEmpty())
	{
		Principal.Identity = TEXT("loopback");
		Principal.Scopes = { EMcpCapabilityScope::Admin };
		Principal.bAuthenticated = true;
	}
	Principal.AuthorityRevision = Revision;
	return Principal;
}
} // namespace McpCapabilityPrincipal

#include "Foundation/McpCapabilityAuthorization.h"

namespace McpAuthorizationCodes
{
const TCHAR* const ScopeNotGranted = TEXT("SCOPE_NOT_GRANTED");
const TCHAR* const ConsentRequired = TEXT("CONSENT_REQUIRED");
const TCHAR* const PathNotPermitted = TEXT("PATH_NOT_PERMITTED");
const TCHAR* const ProjectNotPermitted = TEXT("PROJECT_NOT_PERMITTED");
const TCHAR* const QuotaExceeded = TEXT("QUOTA_EXCEEDED");
const TCHAR* const CommandBlocked = TEXT("COMMAND_BLOCKED");
} // namespace McpAuthorizationCodes

FMcpAuthorizationDecision FMcpAuthorizationDecision::Allow()
{
	return FMcpAuthorizationDecision{};
}

FMcpAuthorizationDecision FMcpAuthorizationDecision::Deny(const TCHAR* Code, const FString& InMessage)
{
	FMcpAuthorizationDecision Decision;
	Decision.bAllowed = false;
	Decision.ErrorCode = Code;
	Decision.Message = InMessage;
	return Decision;
}

namespace McpCapabilityAuthorization
{
namespace
{
FString GrantedScopeList(const FMcpCapabilityPrincipal& Principal)
{
	if (Principal.Scopes.Num() == 0)
	{
		return TEXT("none");
	}
	TArray<FString> Names;
	for (const EMcpCapabilityScope Scope : Principal.Scopes)
	{
		Names.Add(McpCapabilityPrincipal::ScopeToString(Scope));
	}
	return FString::Join(Names, TEXT(","));
}

// Normalize for comparison only: trailing slashes are insignificant.
FString NormalizeForContainment(const FString& Value)
{
	FString Normalized = Value;
	while (Normalized.EndsWith(TEXT("/")) && Normalized.Len() > 1)
	{
		Normalized.LeftChopInline(1);
	}
	return Normalized;
}
} // namespace

FMcpAuthorizationDecision CheckScope(
	const FMcpCapabilityPrincipal& Principal, const FMcpCapabilityDemand& Demand)
{
	if (Principal.IsScopeAuthorized(Demand.RequiredScope))
	{
		return FMcpAuthorizationDecision::Allow();
	}
	FMcpAuthorizationDecision Decision = FMcpAuthorizationDecision::Deny(
		McpAuthorizationCodes::ScopeNotGranted,
		FString::Printf(
			TEXT("Scope '%s' is required for '%s'; this principal holds [%s]."),
			*McpCapabilityPrincipal::ScopeToString(Demand.RequiredScope),
			Demand.CapabilityId.IsEmpty() ? TEXT("this action") : *Demand.CapabilityId,
			*GrantedScopeList(Principal)));
	Decision.RequiredScope = McpCapabilityPrincipal::ScopeToString(Demand.RequiredScope);
	for (const EMcpCapabilityScope Scope : Principal.Scopes)
	{
		Decision.GrantedScopes.Add(McpCapabilityPrincipal::ScopeToString(Scope));
	}
	return Decision;
}

FMcpAuthorizationDecision CheckConsent(
	const FMcpCapabilityDemand& Demand, const FMcpAuthorizationGrant& Grant)
{
	const FString Mode = Demand.ConsentMode.IsEmpty() ? TEXT("none") : Demand.ConsentMode.ToLower();
	if (Mode == TEXT("none"))
	{
		return FMcpAuthorizationDecision::Allow();
	}

	auto Refuse = [&Demand, &Mode]()
	{
		FMcpAuthorizationDecision Decision = FMcpAuthorizationDecision::Deny(
			McpAuthorizationCodes::ConsentRequired,
			FString::Printf(
				TEXT("Capability '%s' requires '%s' consent naming that exact capability."),
				Demand.CapabilityId.IsEmpty() ? TEXT("this action") : *Demand.CapabilityId,
				*Mode));
		Decision.ConsentScope = Mode;
		return Decision;
	};

	// A grant authorizes only the capability it names. Consent is never inferred
	// from loopback, a prior call, idempotency or preview.
	if (!Grant.bConsentPresent || Grant.ConsentCapability != Demand.CapabilityId ||
		Demand.CapabilityId.IsEmpty())
	{
		return Refuse();
	}

	const FString Acknowledge = Grant.ConsentAcknowledge.ToLower();
	if (Mode == TEXT("elevated"))
	{
		return Acknowledge == TEXT("elevated") ? FMcpAuthorizationDecision::Allow() : Refuse();
	}
	// "explicit" is satisfied by an explicit or a stronger elevated acknowledgement.
	const bool bSatisfied = Acknowledge == TEXT("explicit") || Acknowledge == TEXT("elevated");
	return bSatisfied ? FMcpAuthorizationDecision::Allow() : Refuse();
}

bool IsPathWithinPrefix(const FString& Path, const FString& Prefix)
{
	const FString NormalizedPath = NormalizeForContainment(Path);
	const FString NormalizedPrefix = NormalizeForContainment(Prefix);
	if (NormalizedPrefix.IsEmpty())
	{
		return false;
	}
	if (NormalizedPath.Equals(NormalizedPrefix, ESearchCase::IgnoreCase))
	{
		return true;
	}
	// Boundary-aware: only a real path separator may follow the prefix, so
	// "/Game/Team" never admits "/Game/TeamOther".
	return NormalizedPath.StartsWith(NormalizedPrefix + TEXT("/"), ESearchCase::IgnoreCase);
}

FMcpAuthorizationDecision CheckPaths(
	const FMcpCapabilityPrincipal& Principal, const TArray<FString>& Paths)
{
	if (!Principal.IsPathRestricted())
	{
		return FMcpAuthorizationDecision::Allow();
	}
	for (const FString& Path : Paths)
	{
		// Collection hands back a canonical path for everything it could trust.
		// A residual traversal, colon or backslash therefore means the value had
		// no trustworthy canonical form — and prefix matching would MISREAD it:
		// "/Game/TeamA/../TeamB" starts with "/Game/TeamA/" yet resolves outside.
		if (Path.Contains(TEXT("..")) || Path.Contains(TEXT(":")) || Path.Contains(TEXT("\\")))
		{
			return FMcpAuthorizationDecision::Deny(
				McpAuthorizationCodes::PathNotPermitted,
				TEXT("A path in this request is not in canonical form and was refused."));
		}
		bool bPermitted = false;
		for (const FString& Prefix : Principal.AllowedPathPrefixes)
		{
			if (IsPathWithinPrefix(Path, Prefix))
			{
				bPermitted = true;
				break;
			}
		}
		if (!bPermitted)
		{
			return FMcpAuthorizationDecision::Deny(
				McpAuthorizationCodes::PathNotPermitted,
				FString::Printf(
					TEXT("Path '%s' is outside the allowed prefixes for this principal."), *Path));
		}
	}
	return FMcpAuthorizationDecision::Allow();
}

FMcpAuthorizationDecision CheckPathCoverage(
	const FMcpCapabilityPrincipal& Principal,
	const FMcpCapabilityDemand& Demand,
	const FMcpPayloadPathScan& Scan)
{
	if (!Principal.IsPathRestricted())
	{
		return FMcpAuthorizationDecision::Allow();
	}
	// An incomplete scan is not evidence of absence. Padding the payload past the
	// node budget, or nesting the real path below the depth limit, would
	// otherwise collect nothing and be admitted.
	if (Scan.bTruncated)
	{
		return FMcpAuthorizationDecision::Deny(
			McpAuthorizationCodes::PathNotPermitted,
			TEXT("This payload is too large or too deeply nested for path confinement to "
				 "verify completely, so it was refused. Send fewer values or flatten it."));
	}
	const bool bMutating = Demand.RequiredScope == EMcpCapabilityScope::Write ||
		Demand.RequiredScope == EMcpCapabilityScope::Destructive;
	if (!bMutating || !Demand.bDeclaresPathParameter || Scan.Paths.Num() > 0)
	{
		return FMcpAuthorizationDecision::Allow();
	}
	// Nothing was collected, yet the capability declares somewhere to write. The
	// target therefore comes from a server-side default, from a folder/name join
	// the scan cannot see, or from the open editor world — none of which
	// confinement can prove is inside a prefix.
	return FMcpAuthorizationDecision::Deny(
		McpAuthorizationCodes::PathNotPermitted,
		TEXT("This capability writes to a path parameter that the request did not supply, "
			 "so its target cannot be proven inside the allowed prefixes for this "
			 "principal. Supply the path parameter explicitly."));
}

FMcpAuthorizationDecision CheckProject(
	const FMcpCapabilityPrincipal& Principal, const FString& ProjectName)
{
	if (!Principal.IsProjectRestricted())
	{
		return FMcpAuthorizationDecision::Allow();
	}
	for (const FString& Allowed : Principal.AllowedProjects)
	{
		if (Allowed.Equals(ProjectName, ESearchCase::IgnoreCase))
		{
			return FMcpAuthorizationDecision::Allow();
		}
	}
	return FMcpAuthorizationDecision::Deny(
		McpAuthorizationCodes::ProjectNotPermitted,
		FString::Printf(TEXT("Project '%s' is not in the allowed project list for this principal."),
			*ProjectName));
}

} // namespace McpCapabilityAuthorization

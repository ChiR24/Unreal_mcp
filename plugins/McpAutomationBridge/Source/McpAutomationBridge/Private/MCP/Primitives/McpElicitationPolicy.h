// McpElicitationPolicy.h
// Task 38 lane D (native mirror): the elicitation-decision policy for the native
// /mcp surface. Native counterpart of src/server/tool-registry-elicitation.ts.
// Metadata/decision only: NO transport wiring, NO server-initiated RPC, NO new
// MCP method. It answers the two questions the TS policy answers so that
// cross-transport elicitation-decision parity can be established:
//   1. Is a field SAFE to elicit? (never a secret/token/credential, never a
//      destructive-confirmation value.)
//   2. What is the bounded outcome of a high-impact consent request? (granted
//      only on an explicit boolean consent==true; unsupported when the client
//      cannot elicit; declined otherwise.)
// It never reads a field's value and never logs a field name or a token.
#pragma once

#include "CoreMinimal.h"

namespace McpElicitationPolicyInternal
{
	// Case-insensitive substring test mirroring the TS SECRET_FIELD /
	// DESTRUCTIVE_FIELD regexes: a needle matches anywhere in the field name. The
	// caller lowers the field once, so the compare is CaseSensitive over lowercase.
	inline bool ContainsAny(const FString& Lower, const TArray<FString>& Needles)
	{
		for (const FString& Needle : Needles)
		{
			if (Lower.Contains(Needle, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	// Secret/credential markers. Mirrors SECRET_FIELD in tool-registry-elicitation.ts;
	// the api[-_]?key / private[-_]?key / access[-_]?key alternations are expanded
	// to every separator form so a substring compare is faithful to the regex.
	inline const TArray<FString>& SecretNeedles()
	{
		static const TArray<FString> Needles = {
			TEXT("token"), TEXT("secret"), TEXT("password"), TEXT("passwd"),
			TEXT("credential"), TEXT("apikey"), TEXT("api_key"), TEXT("api-key"),
			TEXT("privatekey"), TEXT("private_key"), TEXT("private-key"),
			TEXT("bearer"), TEXT("authorization"),
			TEXT("accesskey"), TEXT("access_key"), TEXT("access-key")
		};
		return Needles;
	}

	// Destructive-confirmation markers. Mirrors DESTRUCTIVE_FIELD.
	inline const TArray<FString>& DestructiveNeedles()
	{
		static const TArray<FString> Needles = {
			TEXT("confirm"), TEXT("force"), TEXT("delete"), TEXT("destroy"),
			TEXT("purge"), TEXT("wipe"), TEXT("overwrite"), TEXT("drop")
		};
		return Needles;
	}
}

// A field is safe to elicit only when its name implies neither a secret nor a
// destructive confirmation. Mirrors isSafeToElicit(): the name is lowered once
// and matched by substring; the field's VALUE is never read or logged.
inline bool McpIsSafeToElicitField(const FString& FieldName)
{
	const FString Lower = FieldName.ToLower();
	return !McpElicitationPolicyInternal::ContainsAny(Lower, McpElicitationPolicyInternal::SecretNeedles())
		&& !McpElicitationPolicyInternal::ContainsAny(Lower, McpElicitationPolicyInternal::DestructiveNeedles());
}

// The bounded reason for a high-impact consent outcome. Mirrors the TS
// ConsentDecision.reason union. Unsupported = the client cannot elicit at all;
// Declined = it could but consent was not granted. Both keep the op BLOCKED.
enum class EMcpConsentReason : uint8
{
	Granted,
	Declined,
	Unsupported
};

// A bounded, typed consent outcome. Mirrors the TS ConsentDecision. Consent is
// never assumed: only an explicit boolean true grants it.
struct FMcpConsentDecision
{
	bool bGranted = false;
	EMcpConsentReason Reason = EMcpConsentReason::Unsupported;
};

// Decide the outcome of a high-impact (destructive/irreversible) consent request.
// Pure mirror of elicitHighImpactConsent()'s decision core, with the transport
// elicitation reduced to its two observable inputs:
//   bHasElicitation  - the client STRUCTURALLY supports elicitation (never
//                      inferred from loopback/idempotency).
//   bClientResponded - the client returned a well-formed elicitation response.
//   bConsentValue    - the single boolean `consent` field it returned.
// Granted ONLY when the client supports elicitation, responded, and consent==true;
// unsupported when it cannot elicit; declined otherwise. It inspects neither a
// secret nor the destructive value itself, and logs nothing.
inline FMcpConsentDecision McpEvaluateHighImpactConsent(bool bHasElicitation, bool bClientResponded, bool bConsentValue)
{
	FMcpConsentDecision Decision;
	if (!bHasElicitation)
	{
		Decision.bGranted = false;
		Decision.Reason = EMcpConsentReason::Unsupported;
		return Decision;
	}
	if (bClientResponded && bConsentValue)
	{
		Decision.bGranted = true;
		Decision.Reason = EMcpConsentReason::Granted;
		return Decision;
	}
	Decision.bGranted = false;
	Decision.Reason = EMcpConsentReason::Declined;
	return Decision;
}

// The single boolean field a high-impact consent request asks for. Mirrors the TS
// consent schema: one boolean, never a secret and never the destructive value.
inline const FString& McpHighImpactConsentField()
{
	static const FString Field = TEXT("consent");
	return Field;
}

#include "Core/Security/McpPrequeueGate.h"

#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

// Capability-demand resolution for the pre-queue gate.
//
// Split out of McpPrequeueGate.cpp so both stay under the plugin's 250 pure-line
// ceiling. The security property this file owns: THE GATE AND THE DISPATCHER
// MUST RESOLVE THE SAME ACTION.
//
// It does that by construction rather than by agreement — it calls the very
// function the dispatch macros call, McpHandlerUtils::NormalizeAction, which
// reads `payload.subAction` and otherwise falls back to the envelope action.
// It deliberately does NOT read `payload.action`, so a decoy value there can
// never lower the demand below what NormalizeAction resolves.
//
// That is only half the property, because `payload.action` is NOT an unread
// field: roughly ten domain dispatchers (system_control, control_actor,
// control_editor, manage_level, environment, animation, manage_input, effect,
// blueprint) resolve their sub-action from it. Reading it here would let a
// client lower its own scope; ignoring it while a dispatcher honours it would
// let a client RAISE what runs past what was authorized. Neither is acceptable,
// so the other half is enforced before this runs:
// FMcpConnectionManager::AuthorizeAutomationRequest NORMALIZES any
// automation_request whose payload declares both fields with different values
// (overwriting `action` from the authoritative `subAction`), and
// McpNativeTransportGatewayExecute stamps `subAction`
// unconditionally from the server-resolved action. By the time a payload reaches
// this function the two fields cannot disagree — do not remove either guard
// without making every dispatcher call NormalizeAction.

namespace McpPrequeueGate
{
namespace
{
int32 ScopeStrictness(EMcpCapabilityScope Scope)
{
	switch (Scope)
	{
	case EMcpCapabilityScope::Read:
		return 0;
	case EMcpCapabilityScope::Write:
		return 1;
	case EMcpCapabilityScope::Destructive:
		return 2;
	case EMcpCapabilityScope::Admin:
		return 3;
	}
	return 3;
}

// An unrecognised scope string is treated as Admin, so a catalogue that ever
// grows a new scope name cannot silently downgrade a demand.
EMcpCapabilityScope ScopeFromString(const FString& Value)
{
	const FString Lower = Value.ToLower();
	if (Lower == TEXT("read")) return EMcpCapabilityScope::Read;
	if (Lower == TEXT("write")) return EMcpCapabilityScope::Write;
	if (Lower == TEXT("destructive")) return EMcpCapabilityScope::Destructive;
	return EMcpCapabilityScope::Admin;
}

int32 ConsentStrictness(const FString& Mode)
{
	const FString Lower = Mode.ToLower();
	if (Lower == TEXT("elevated")) return 2;
	if (Lower == TEXT("explicit")) return 1;
	return 0;
}

void ReadPolicy(const FMcpCapabilityRecord& Record, EMcpCapabilityScope& OutScope, FString& OutConsent)
{
	OutScope = EMcpCapabilityScope::Admin;
	OutConsent = TEXT("none");
	if (!Record.Policy.IsValid())
	{
		return;
	}
	FString ScopeText;
	if (Record.Policy->TryGetStringField(TEXT("requiredScope"), ScopeText))
	{
		OutScope = ScopeFromString(ScopeText);
	}
	FString ConsentText;
	if (Record.Policy->TryGetStringField(TEXT("consent"), ConsentText))
	{
		OutConsent = ConsentText;
	}
}

// Does this capability declare somewhere to write? Answered from the canonical
// record's own input schema, using the SAME key predicate payload collection
// uses, so "path parameter" cannot mean two different things on the two sides of
// the gate. FAIL CLOSED when the schema is missing or unreadable.
bool DeclaresPathParameter(const FMcpCapabilityRecord& Record)
{
	if (!Record.InputSchema.IsValid())
	{
		return true;
	}
	const TSharedPtr<FJsonObject>* Properties = nullptr;
	if (!Record.InputSchema->TryGetObjectField(TEXT("properties"), Properties) || !Properties)
	{
		return true;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>> Pair : (*Properties)->Values)
	{
		if (McpCapabilityAuthorization::IsPathParameterKey(Pair.Key))
		{
			return true;
		}
	}
	return false;
}

FString ActionSuffix(const FString& CapabilityId)
{
	int32 DotIndex = INDEX_NONE;
	if (CapabilityId.FindLastChar(TEXT('.'), DotIndex))
	{
		return CapabilityId.RightChop(DotIndex + 1);
	}
	return CapabilityId;
}

// Exactly what MCP_DISPATCH_SUBACTION will resolve for this request.
FString ResolveDispatchedAction(const FMcpPrequeueRequest& Request)
{
	return McpHandlerUtils::NormalizeAction(Request.DispatchAction, Request.Payload);
}

bool FindById(const FString& Id, const FMcpCapabilityStore& Store, FMcpCapabilityDemand& OutDemand)
{
	for (const FMcpCapabilityRecord& Record : Store.GetRecords())
	{
		if (Record.Id.Equals(Id, ESearchCase::CaseSensitive))
		{
			ReadPolicy(Record, OutDemand.RequiredScope, OutDemand.ConsentMode);
			OutDemand.CapabilityId = Record.Id;
			OutDemand.bDeclaresPathParameter = DeclaresPathParameter(Record);
			return true;
		}
	}
	return false;
}
} // namespace

FMcpCapabilityDemand ResolveDemand(const FMcpPrequeueRequest& Request)
{
	// Defaults are Admin / no-consent: every path that fails to identify a
	// catalogue record leaves the demand at Admin.
	FMcpCapabilityDemand Demand;

	const FMcpCapabilityStore& Store = FMcpCapabilityStore::Get();
	if (!Store.IsReady())
	{
		return Demand;
	}

	if (!Request.CapabilityId.IsEmpty())
	{
		FindById(Request.CapabilityId, Store, Demand);
		return Demand;
	}

	const FString Specific = ResolveDispatchedAction(Request);
	if (Specific.IsEmpty())
	{
		return Demand;
	}

	TArray<const FMcpCapabilityRecord*> Candidates;
	for (const FMcpCapabilityRecord& Record : Store.GetRecords())
	{
		if (ActionSuffix(Record.Id).Equals(Specific, ESearchCase::IgnoreCase))
		{
			Candidates.Add(&Record);
		}
	}

	// Narrow by the dispatch target. FAIL CLOSED when nothing agrees: a request
	// whose action does not belong to the parent tool it was sent to keeps the
	// Admin default, instead of borrowing an unrelated parent's cheaper policy.
	if (!Request.DispatchAction.IsEmpty())
	{
		TArray<const FMcpCapabilityRecord*> Narrowed;
		for (const FMcpCapabilityRecord* Record : Candidates)
		{
			if (Record->Parent.Equals(Request.DispatchAction, ESearchCase::IgnoreCase) ||
				Record->DispatchAction.Equals(Request.DispatchAction, ESearchCase::IgnoreCase))
			{
				Narrowed.Add(Record);
			}
		}
		if (Narrowed.Num() == 0)
		{
			return Demand;
		}
		Candidates = MoveTemp(Narrowed);
	}

	if (Candidates.Num() == 0)
	{
		return Demand;
	}

	// Ambiguity can only RAISE the bar: the strictest scope and the strictest
	// consent among the candidates win, and the capability that carries the
	// strictest consent is the one a grant must name.
	int32 BestScope = -1;
	int32 BestConsent = -1;
	// Only now may this leave its fail-closed default: one candidate declaring a
	// path parameter is enough to demand a provable target from every candidate.
	Demand.bDeclaresPathParameter = false;
	for (const FMcpCapabilityRecord* Record : Candidates)
	{
		Demand.bDeclaresPathParameter |= DeclaresPathParameter(*Record);
		EMcpCapabilityScope Scope = EMcpCapabilityScope::Admin;
		FString Consent;
		ReadPolicy(*Record, Scope, Consent);
		if (ScopeStrictness(Scope) > BestScope)
		{
			BestScope = ScopeStrictness(Scope);
			Demand.RequiredScope = Scope;
		}
		if (ConsentStrictness(Consent) > BestConsent)
		{
			BestConsent = ConsentStrictness(Consent);
			Demand.ConsentMode = Consent;
			Demand.CapabilityId = Record->Id;
		}
	}
	return Demand;
}

FString ResolveActionClass(const FString& DispatchAction, const TSharedPtr<FJsonObject>& Payload)
{
	FMcpPrequeueRequest Request;
	Request.DispatchAction = DispatchAction;
	Request.Payload = Payload;

	const FMcpCapabilityDemand Demand = ResolveDemand(Request);
	if (Demand.CapabilityId.IsEmpty())
	{
		// Nothing in the catalogue matched. The gate answers Admin here because
		// it must fail closed; telemetry answers "unknown" because reporting an
		// unmatched action as administrative would misstate what ran.
		return TEXT("unknown");
	}

	switch (Demand.RequiredScope)
	{
	case EMcpCapabilityScope::Read:
		return TEXT("read");
	case EMcpCapabilityScope::Write:
		return TEXT("write");
	case EMcpCapabilityScope::Destructive:
		return TEXT("destructive");
	default:
		return TEXT("admin");
	}
}
} // namespace McpPrequeueGate

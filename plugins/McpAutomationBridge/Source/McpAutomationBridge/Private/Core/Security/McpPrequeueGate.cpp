#include "Core/Security/McpPrequeueGate.h"

#include "Domains/ConsoleCommand/McpAutomationBridge_ConsoleCommandHandlersPrivate.h"
#include "Foundation/McpPrincipalQuota.h"
#include "Misc/App.h"

// Demand resolution lives in the sibling McpPrequeueDemand.cpp; this file owns
// the refusal ORDER and the checks that need the console-command policy.

namespace McpPrequeueGate
{
namespace
{
// Bounds mirror the payload path scan: a hostile client cannot make the gate
// walk an unbounded structure before its request is even queued.
constexpr int32 MaxScanDepth = 8;
constexpr int32 MaxScanNodes = 4096;

FMcpAuthorizationGrant ReadGrant(const TSharedPtr<FJsonObject>& Consent)
{
	FMcpAuthorizationGrant Grant;
	if (!Consent.IsValid())
	{
		return Grant;
	}
	Grant.bConsentPresent = true;
	Consent->TryGetStringField(TEXT("capability"), Grant.ConsentCapability);
	Consent->TryGetStringField(TEXT("acknowledge"), Grant.ConsentAcknowledge);
	return Grant;
}

// Mirrors FMcpPayloadPathScan: the budget is a bound, and hitting it is an
// answer the scan must report rather than silently treat as "nothing found".
struct FCommandScanState
{
	int32 NodeBudget = MaxScanNodes;
	bool bTruncated = false;
};

bool ScanForBlockedCommand(
	const TSharedPtr<FJsonValue>& Value, const FString& Key, int32 Depth, FCommandScanState& State);

bool ObjectHasBlockedCommand(
	const TSharedPtr<FJsonObject>& Object, int32 Depth, FCommandScanState& State)
{
	if (!Object.IsValid())
	{
		return false;
	}
	if (Depth > MaxScanDepth)
	{
		State.bTruncated = true;
		return false;
	}
	for (const TPair<FString, TSharedPtr<FJsonValue>> Pair : Object->Values)
	{
		if (State.NodeBudget <= 0)
		{
			State.bTruncated = true;
			return false;
		}
		if (ScanForBlockedCommand(Pair.Value, Pair.Key, Depth + 1, State))
		{
			return true;
		}
	}
	return false;
}

// Recursive, not top-level-only: a nested `command` reaches the same console
// executor as a top-level one, so the pre-queue check has to follow it there.
bool ScanForBlockedCommand(
	const TSharedPtr<FJsonValue>& Value, const FString& Key, int32 Depth, FCommandScanState& State)
{
	if (!Value.IsValid())
	{
		return false;
	}
	if (Depth > MaxScanDepth || State.NodeBudget <= 0)
	{
		State.bTruncated = true;
		return false;
	}
	--State.NodeBudget;

	const FString LowerKey = Key.ToLower();
	// Every key the executors actually read. `cmd` is the per-entry key the batch
	// executor accepts (McpAutomationBridge_ConsoleCommandBatch.cpp reads
	// TryGetStringField(TEXT("cmd"))), so omitting it here let a blocked command
	// wrapped as {"commands":[{"cmd":"..."}]} pass the PRE-queue gate and consume
	// editor-thread work. The post-queue check still refused execution, so this
	// closes a defence-in-depth gap rather than an execution hole -- but the gate
	// exists precisely so such a request never reaches the queue.
	const bool bCommandKey = LowerKey == TEXT("command") || LowerKey == TEXT("commands")
		|| LowerKey == TEXT("cmd");

	if (Value->Type == EJson::String)
	{
		return bCommandKey && ConsoleCommandSecurity::IsBlockedCommand(Value->AsString());
	}
	if (Value->Type == EJson::Object)
	{
		return ObjectHasBlockedCommand(Value->AsObject(), Depth, State);
	}
	if (Value->Type == EJson::Array)
	{
		for (const TSharedPtr<FJsonValue>& Element : Value->AsArray())
		{
			if (ScanForBlockedCommand(Element, Key, Depth + 1, State))
			{
				return true;
			}
		}
	}
	return false;
}

// Task 22 console-command policy, applied BEFORE the queue. The in-handler check
// in Domains/ConsoleCommand stays as post-queue defence in depth; both call the
// same ConsoleCommandSecurity::IsBlockedCommand predicate, so they cannot drift.
FMcpAuthorizationDecision CheckConsoleCommands(const TSharedPtr<FJsonObject>& Payload)
{
	FCommandScanState State;
	if (ObjectHasBlockedCommand(Payload, 0, State))
	{
		// The rejected command is deliberately NOT echoed: it is attacker-controlled
		// text and the refusal reason is the same for every blocked command.
		return FMcpAuthorizationDecision::Deny(McpAuthorizationCodes::CommandBlocked,
			TEXT("Console command is blocked by policy and was refused before dispatch."));
	}
	// "I ran out of budget" is not "I found nothing". Padding a payload past the
	// bound would otherwise hide a blocked command from this check entirely.
	if (State.bTruncated)
	{
		return FMcpAuthorizationDecision::Deny(McpAuthorizationCodes::CommandBlocked,
			TEXT("This payload is too large or too deeply nested to check against the "
				 "console-command policy, so it was refused. Send fewer values or flatten it."));
	}
	return FMcpAuthorizationDecision::Allow();
}
} // namespace

namespace
{
FMcpAuthorizationDecision AuthorizeWithDemand(
	const FMcpPrequeueRequest& Request, const FMcpCapabilityDemand& Demand)
{
	const FMcpCapabilityPrincipal& Principal = *Request.Principal;

	FMcpAuthorizationDecision Decision =
		McpCapabilityAuthorization::CheckScope(Principal, Demand);
	if (!Decision.bAllowed) return Decision;

	Decision = McpCapabilityAuthorization::CheckConsent(Demand, ReadGrant(Request.Consent));
	if (!Decision.bAllowed) return Decision;

	Decision = McpCapabilityAuthorization::CheckProject(Principal, FApp::GetProjectName());
	if (!Decision.bAllowed) return Decision;

	// Strict collection for a path-restricted principal only, so an unrestricted
	// principal keeps its previous behaviour exactly.
	const FMcpPayloadPathScan Scan = McpCapabilityAuthorization::CollectPayloadPaths(
		Request.Payload, Principal.IsPathRestricted());

	Decision = McpCapabilityAuthorization::CheckPaths(Principal, Scan.Paths);
	if (!Decision.bAllowed) return Decision;

	// Every path the scan DID collect is in-prefix. Coverage asks the second,
	// harder question: was the scan complete, and did it see a target at all?
	Decision = McpCapabilityAuthorization::CheckPathCoverage(Principal, Demand, Scan);
	if (!Decision.bAllowed) return Decision;

	Decision = CheckConsoleCommands(Request.Payload);
	if (!Decision.bAllowed) return Decision;

	FString QuotaReason;
	if (!FMcpPrincipalQuotaLedger::Get().TryCharge(Principal, Request.bIsToolCall, QuotaReason))
	{
		FMcpAuthorizationDecision Quota =
			FMcpAuthorizationDecision::Deny(McpAuthorizationCodes::QuotaExceeded, QuotaReason);
		// The only policy refusal that succeeds on a later retry, once the
		// principal's window rolls; every other refusal is permanent as configured.
		Quota.bRetryable = true;
		return Quota;
	}
	return FMcpAuthorizationDecision::Allow();
}

FMcpAuthorizationDecision RequirePrincipal(const FMcpPrequeueRequest& Request)
{
	if (Request.Principal)
	{
		return FMcpAuthorizationDecision::Allow();
	}
	return FMcpAuthorizationDecision::Deny(McpAuthorizationCodes::ScopeNotGranted,
		TEXT("No capability principal is bound to this connection."));
}
} // namespace

FMcpAuthorizationDecision Authorize(const FMcpPrequeueRequest& Request)
{
	const FMcpAuthorizationDecision Bound = RequirePrincipal(Request);
	if (!Bound.bAllowed) return Bound;
	return AuthorizeWithDemand(Request, ResolveDemand(Request));
}

FMcpAuthorizationDecision AuthorizeRead(const FMcpPrequeueRequest& Request)
{
	const FMcpAuthorizationDecision Bound = RequirePrincipal(Request);
	if (!Bound.bAllowed) return Bound;

	FMcpCapabilityDemand Demand;
	Demand.RequiredScope = EMcpCapabilityScope::Read;
	Demand.ConsentMode = TEXT("none");
	return AuthorizeWithDemand(Request, Demand);
}
} // namespace McpPrequeueGate

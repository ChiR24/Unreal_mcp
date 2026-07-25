#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#include "Foundation/McpCapabilityAuthorization.h"

// Task 40 pre-queue authorization gate — the ONE place both transports ask
// "may this principal run this request?" before anything is enqueued.
//
// It lives in Core because it is the only layer allowed to see all three of the
// canonical capability catalogue (MCP/Gateway), the Task 22 console-command
// policy (Domains) and the shared Foundation predicates. Foundation stays free
// of those dependencies so its predicates remain unit-testable in isolation.
//
// Every refusal here happens BEFORE UMcpAutomationBridgeSubsystem::
// QueueAutomationRequest, so a refused request performs no editor work at all.
// The existing in-handler checks (console-command blocklist, path sanitizers)
// are deliberately left in place as post-queue defence in depth.

struct FMcpPrequeueRequest
{
	const FMcpCapabilityPrincipal* Principal = nullptr;

	// Set only when the SERVER resolved an exact capability (native /mcp knows
	// Plan.CapabilityId). Never populated from a client-supplied field, because a
	// client could otherwise name a cheap capability to lower its own demand.
	FString CapabilityId;

	// The WebSocket envelope's `action`: a dispatch target such as
	// "control_actor", not necessarily a capability id.
	FString DispatchAction;

	TSharedPtr<FJsonObject> Payload;

	// The automation_request envelope sibling. Re-validated here; the plugin
	// never trusts the TypeScript layer to have checked it.
	TSharedPtr<FJsonObject> Consent;

	// False for a discovery request (native resources/*, prompts/*, tools/list),
	// which consumes the principal's request budget but not its tool-call budget.
	// This is what makes MaxRequestsPerMinute and MaxToolCallsPerMinute distinct
	// settings rather than a collapsed min().
	bool bIsToolCall = true;
};

namespace McpPrequeueGate
{
// Resolve what the request demands from the canonical catalogue.
//
// FAIL-CLOSED by construction: an action with no catalogue match demands Admin,
// so a raw WebSocket client naming an unknown or misspelled action is refused
// unless it holds Admin. When several records are consistent with what the
// client sent, the STRICTEST demand among them wins, so ambiguity can never
// lower the bar.
FMcpCapabilityDemand ResolveDemand(const FMcpPrequeueRequest& Request);

// Full gate in refusal order: scope, consent, project, path, console command,
// then quota. Quota is charged last so a request refused on authorization does
// not consume the principal's budget.
FMcpAuthorizationDecision Authorize(const FMcpPrequeueRequest& Request);

// The same gate for a request that reads project data without naming a
// capability — the native MCP primitives (resources/*, prompts/*, tools/list).
// The demand is a fixed `read` with no consent, because there is no catalogue
// record to resolve; every other check (project, path, quota) is identical.
FMcpAuthorizationDecision AuthorizeRead(const FMcpPrequeueRequest& Request);
} // namespace McpPrequeueGate

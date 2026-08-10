#include "Core/Security/McpPrequeueGate.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Foundation/McpPrincipalQuota.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

// Regression tests for the Task 40 pre-queue gate.
//
// The gate previously had NO tests at all, so deleting its central security
// claims broke nothing. Two of these cases are the exact bypasses a review
// found: a decoy `payload.action` that steered the required scope away from the
// action actually dispatched, and a `/Content` alias that walked past path
// confinement and was rooted at `/Game` afterwards by the handler.
//
// Every negative assertion is paired with a positive control so it cannot pass
// vacuously (for example by refusing everything).

namespace
{
FMcpCapabilityPrincipal MakeGatePrincipal(
	const TArray<EMcpCapabilityScope>& Scopes, const TArray<FString>& Prefixes = {})
{
	FMcpCapabilityPrincipal Principal;
	Principal.Identity = TEXT("scoped:gate-test");
	Principal.Scopes = Scopes;
	Principal.AllowedPathPrefixes = Prefixes;
	Principal.bAuthenticated = true;
	return Principal;
}

TSharedPtr<FJsonObject> GatePayload(const TMap<FString, FString>& Fields)
{
	TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Field : Fields)
	{
		Object->SetStringField(Field.Key, Field.Value);
	}
	return Object;
}

FMcpPrequeueRequest MakeGateRequest(const FString& DispatchAction, const TSharedPtr<FJsonObject>& InPayload)
{
	FMcpPrequeueRequest Request;
	Request.DispatchAction = DispatchAction;
	Request.Payload = InPayload;
	return Request;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpPrequeueGateDemandTest,
	"McpAutomationBridge.Foundation.PrequeueGate.Demand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPrequeueGateDemandTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// Without a loaded catalogue every demand degrades to Admin and the cases
	// below would pass for the wrong reason.
	if (!TestTrue(TEXT("capability store is ready"), FMcpCapabilityStore::Get().IsReady()))
	{
		return false;
	}

	// BLOCKER 1 REGRESSION — a decoy `payload.action` must not steer the demand.
	// `console_command` is write-scoped; `find_by_class` is read-scoped. The
	// dispatcher resolves subAction, so the gate must too.
	{
		const FMcpCapabilityDemand Honest = McpPrequeueGate::ResolveDemand(MakeGateRequest(
			TEXT("system_control"), GatePayload({ { TEXT("subAction"), TEXT("console_command") } })));
		TestTrue(TEXT("positive control: console_command demands write"),
			Honest.RequiredScope == EMcpCapabilityScope::Write);

		const FMcpCapabilityDemand Decoyed = McpPrequeueGate::ResolveDemand(MakeGateRequest(
			TEXT("system_control"),
			GatePayload({ { TEXT("subAction"), TEXT("console_command") },
					  { TEXT("action"), TEXT("find_by_class") },
					  { TEXT("command"), TEXT("obliterate") } })));
		TestTrue(TEXT("a decoy payload.action does NOT downgrade the required scope"),
			Decoyed.RequiredScope == EMcpCapabilityScope::Write);
		TestTrue(TEXT("the decoy does not replace the resolved capability"),
			Decoyed.CapabilityId == Honest.CapabilityId);
	}

	// An action with no catalogue match demands Admin.
	{
		const FMcpCapabilityDemand Demand = McpPrequeueGate::ResolveDemand(MakeGateRequest(
			TEXT("system_control"), GatePayload({ { TEXT("subAction"), TEXT("no_such_action_xyz") } })));
		TestTrue(TEXT("unknown action demands admin"),
			Demand.RequiredScope == EMcpCapabilityScope::Admin);
	}

	// Ambiguity raises the bar: `delete_node` is destructive on manage_blueprint
	// and write on manage_asset, so an unnarrowed resolve must take destructive.
	{
		const FMcpCapabilityDemand Ambiguous = McpPrequeueGate::ResolveDemand(
			MakeGateRequest(FString(), GatePayload({ { TEXT("subAction"), TEXT("delete_node") } })));
		TestTrue(TEXT("strictest scope wins across ambiguous candidates"),
			Ambiguous.RequiredScope == EMcpCapabilityScope::Destructive);

		const FMcpCapabilityDemand Narrowed = McpPrequeueGate::ResolveDemand(MakeGateRequest(
			TEXT("manage_asset"), GatePayload({ { TEXT("subAction"), TEXT("delete_node") } })));
		TestTrue(TEXT("positive control: narrowing to the real parent keeps its own scope"),
			Narrowed.RequiredScope == EMcpCapabilityScope::Write);
	}

	// BLOCKER 1 REGRESSION — narrowing that matches nothing must fail closed
	// rather than fall back to the un-narrowed candidate list.
	{
		const FMcpCapabilityDemand Demand = McpPrequeueGate::ResolveDemand(MakeGateRequest(
			TEXT("manage_level"), GatePayload({ { TEXT("subAction"), TEXT("delete_node") } })));
		TestTrue(TEXT("an action that does not belong to the named parent demands admin"),
			Demand.RequiredScope == EMcpCapabilityScope::Admin);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpPrequeueGatePathConfinementTest,
	"McpAutomationBridge.Foundation.PrequeueGate.PathConfinement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPrequeueGatePathConfinementTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpCapabilityAuthorization;

	const FMcpCapabilityPrincipal Confined =
		MakeGatePrincipal({ EMcpCapabilityScope::Destructive }, { TEXT("/Game/TeamA") });
	const FMcpCapabilityPrincipal Unconfined = MakeGatePrincipal({ EMcpCapabilityScope::Destructive });

	auto Allows = [&](const FMcpCapabilityPrincipal& Principal, const TSharedPtr<FJsonObject>& P)
	{
		return CheckPaths(Principal, CollectPayloadPaths(P, Principal.IsPathRestricted()).Paths).bAllowed;
	};

	// Positive controls first: confinement must still ADMIT what it should.
	TestTrue(TEXT("control: an in-prefix /Game path is allowed"),
		Allows(Confined, GatePayload({ { TEXT("folderPath"), TEXT("/Game/TeamA/Sub") } })));
	TestTrue(TEXT("control: the prefix itself is allowed"),
		Allows(Confined, GatePayload({ { TEXT("folderPath"), TEXT("/Game/TeamA") } })));
	TestTrue(TEXT("control: a /Content alias INSIDE the prefix is allowed"),
		Allows(Confined, GatePayload({ { TEXT("folderPath"), TEXT("/Content/TeamA/Sub") } })));
	TestTrue(TEXT("control: a non-path scalar is not mistaken for a path"),
		Allows(Confined, GatePayload({ { TEXT("actorName"), TEXT("SomeActor") } })));
	TestTrue(TEXT("control: an OS import path is not content-confined"),
		Allows(Confined, GatePayload({ { TEXT("sourcePath"), TEXT("/home/dev/mesh.fbx") } })));
	TestTrue(TEXT("control: an unrestricted principal is unaffected"),
		Allows(Unconfined, GatePayload({ { TEXT("folderPath"), TEXT("/Content/TeamB") } })));

	// BLOCKER 2 REGRESSION — every alias the handler would root at /Game/TeamB
	// must be refused for a principal confined to /Game/TeamA.
	TestFalse(TEXT("/Content alias is confined"),
		Allows(Confined, GatePayload({ { TEXT("folderPath"), TEXT("/Content/TeamB") } })));
	TestFalse(TEXT("backslash /Content alias is confined"),
		Allows(Confined, GatePayload({ { TEXT("folderPath"), TEXT("\\Content\\TeamB") } })));
	TestFalse(TEXT("lower-case /content alias is confined"),
		Allows(Confined, GatePayload({ { TEXT("folderPath"), TEXT("/content/TeamB") } })));
	TestFalse(TEXT("bare relative path is confined"),
		Allows(Confined, GatePayload({ { TEXT("folderPath"), TEXT("TeamB/Secret") } })));
	TestFalse(TEXT("bare single token under a path key is confined"),
		Allows(Confined, GatePayload({ { TEXT("packagePath"), TEXT("Secret") } })));
	TestFalse(TEXT("doubled separators do not evade confinement"),
		Allows(Confined, GatePayload({ { TEXT("folderPath"), TEXT("//Content//TeamB") } })));
	TestFalse(TEXT("traversal out of the prefix is refused"),
		Allows(Confined, GatePayload({ { TEXT("folderPath"), TEXT("/Game/TeamA/../TeamB") } })));
	TestFalse(TEXT("a sibling prefix is not admitted by string prefix alone"),
		Allows(Confined, GatePayload({ { TEXT("folderPath"), TEXT("/Game/TeamAOther") } })));

	// A nested path is collected exactly like a top-level one.
	{
		TSharedPtr<FJsonObject> Inner = GatePayload({ { TEXT("folderPath"), TEXT("/Content/TeamB") } });
		TSharedPtr<FJsonObject> Outer = MakeShared<FJsonObject>();
		Outer->SetObjectField(TEXT("options"), Inner);
		TestFalse(TEXT("a nested /Content alias is confined"), Allows(Confined, Outer));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpPrequeueGateQuotaOrderTest,
	"McpAutomationBridge.Foundation.PrequeueGate.QuotaChargedLast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPrequeueGateQuotaOrderTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpPrincipalQuotaLedger::Get().Reset();

	// One request per minute, and a scope this principal does not hold.
	FMcpCapabilityPrincipal Principal = MakeGatePrincipal({ EMcpCapabilityScope::Read });
	Principal.Identity = TEXT("scoped:quota-order");
	Principal.MaxRequestsPerMinute = 1;

	FMcpPrequeueRequest Refused = MakeGateRequest(
		TEXT("manage_asset"), GatePayload({ { TEXT("subAction"), TEXT("delete_asset") } }));
	Refused.Principal = &Principal;

	const FMcpAuthorizationDecision First = McpPrequeueGate::Authorize(Refused);
	TestFalse(TEXT("a destructive action is refused for a read principal"), First.bAllowed);
	const FMcpAuthorizationDecision Second = McpPrequeueGate::Authorize(Refused);
	TestTrue(TEXT("the second refusal is still a scope refusal, not a quota refusal"),
		Second.ErrorCode == First.ErrorCode);

	// The budget survived both refusals, so an authorized call still fits.
	FMcpPrequeueRequest Allowed = MakeGateRequest(
		TEXT("control_actor"), GatePayload({ { TEXT("subAction"), TEXT("find_by_class") } }));
	Allowed.Principal = &Principal;
	TestTrue(TEXT("an authorized request still fits the unspent budget"),
		McpPrequeueGate::Authorize(Allowed).bAllowed);

	// ... and only now is the budget gone.
	const FMcpAuthorizationDecision Exhausted = McpPrequeueGate::Authorize(Allowed);
	TestFalse(TEXT("the next authorized request exceeds the quota"), Exhausted.bAllowed);
	TestTrue(TEXT("a quota refusal is retryable"), Exhausted.bRetryable);

	FMcpPrincipalQuotaLedger::Get().Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpPrequeueGateReadDemandTest,
	"McpAutomationBridge.Foundation.PrequeueGate.PrimitiveReadDemand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPrequeueGateReadDemandTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpPrincipalQuotaLedger::Get().Reset();

	const FMcpCapabilityPrincipal Reader = MakeGatePrincipal({ EMcpCapabilityScope::Read });
	const FMcpCapabilityPrincipal Writer = MakeGatePrincipal({ EMcpCapabilityScope::Write });
	const FMcpCapabilityPrincipal Confined =
		MakeGatePrincipal({ EMcpCapabilityScope::Read }, { TEXT("/Game/TeamA") });

	FMcpPrequeueRequest Request;
	Request.Principal = &Reader;
	TestTrue(TEXT("a read principal may run an MCP primitive"),
		McpPrequeueGate::AuthorizeRead(Request).bAllowed);

	// Exact-set semantics: write does NOT imply read, so a write-only token
	// cannot use primitives as a read channel.
	Request.Principal = &Writer;
	TestFalse(TEXT("a write-only principal may NOT read via an MCP primitive"),
		McpPrequeueGate::AuthorizeRead(Request).bAllowed);

	// A resource that addresses a content path is confined like any other path.
	Request.Principal = &Confined;
	Request.Payload = GatePayload({ { TEXT("path"), TEXT("/Game/TeamA/Thing") } });
	TestTrue(TEXT("control: an in-prefix resource path is readable"),
		McpPrequeueGate::AuthorizeRead(Request).bAllowed);
	Request.Payload = GatePayload({ { TEXT("path"), TEXT("/Game/TeamB/Secret") } });
	TestFalse(TEXT("an out-of-prefix resource path is refused"),
		McpPrequeueGate::AuthorizeRead(Request).bAllowed);

	FMcpPrincipalQuotaLedger::Get().Reset();
	return true;
}
#endif

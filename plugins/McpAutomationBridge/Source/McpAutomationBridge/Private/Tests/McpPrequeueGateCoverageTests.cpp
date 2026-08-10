#include "Core/Security/McpPrequeueGate.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"

#include "Foundation/McpPrincipalQuota.h"
#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"

// Regression tests for the two fail-OPEN holes a cycle-2 review found in the
// pre-queue gate.
//
// The bounded payload scans were correct arithmetic with the wrong conclusion:
// on exhaustion they returned "nothing found", which the gate read as "nothing
// to refuse". A client could pad a payload past the node budget, or nest its
// real path below the depth limit, and confinement never saw the path at all.
//
// Separately, value scanning is structurally blind to three things: a
// folder/name join where only the folder was checked, an optional path
// parameter the client OMITS so a server-side default applies, and a
// bare-relative value under a key no allowlist names. No key allowlist can
// reach any of them, because the escaping value is never in the payload.

namespace
{
FMcpCapabilityPrincipal CoveragePrincipal(
	const TArray<EMcpCapabilityScope>& Scopes, const TArray<FString>& Prefixes)
{
	FMcpCapabilityPrincipal Principal;
	Principal.Identity = TEXT("scoped:coverage-test");
	Principal.Scopes = Scopes;
	Principal.AllowedPathPrefixes = Prefixes;
	Principal.bAuthenticated = true;
	return Principal;
}

TSharedPtr<FJsonObject> Fields(const TMap<FString, FString>& Values)
{
	TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Value : Values)
	{
		Object->SetStringField(Value.Key, Value.Value);
	}
	return Object;
}

// More short strings than the scan may visit, so the real path that follows
// them is never reached. This is the >4096-node padding attack.
void AddPadding(const TSharedPtr<FJsonObject>& Payload, int32 Count)
{
	TArray<TSharedPtr<FJsonValue>> Padding;
	Padding.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Padding.Add(MakeShared<FJsonValueString>(TEXT("x")));
	}
	Payload->SetArrayField(TEXT("notes"), Padding);
}

// Wrap a leaf object in `Levels` layers, so the value sits below the depth bound.
TSharedPtr<FJsonObject> Nest(const TSharedPtr<FJsonObject>& Leaf, int32 Levels)
{
	TSharedPtr<FJsonObject> Current = Leaf;
	for (int32 Index = 0; Index < Levels; ++Index)
	{
		TSharedPtr<FJsonObject> Wrapper = MakeShared<FJsonObject>();
		Wrapper->SetObjectField(TEXT("nested"), Current);
		Current = Wrapper;
	}
	return Current;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpPrequeueGateScanTruncationTest,
	"McpAutomationBridge.Foundation.PrequeueGate.ScanTruncationFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPrequeueGateScanTruncationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace McpCapabilityAuthorization;

	const FMcpCapabilityPrincipal Confined =
		CoveragePrincipal({ EMcpCapabilityScope::Write }, { TEXT("/Game/TeamA") });
	const FMcpCapabilityPrincipal Unconfined =
		CoveragePrincipal({ EMcpCapabilityScope::Write }, {});

	FMcpCapabilityDemand Demand;
	Demand.RequiredScope = EMcpCapabilityScope::Write;
	Demand.CapabilityId = TEXT("coverage.test");
	Demand.bDeclaresPathParameter = true;

	// POSITIVE CONTROL: an ordinary payload is complete, and an in-prefix target
	// is admitted. Without this the denials below could be refusing everything.
	{
		const FMcpPayloadPathScan Scan = CollectPayloadPaths(
			Fields({ { TEXT("folderPath"), TEXT("/Game/TeamA/Sub") } }), true);
		TestFalse(TEXT("control: a small payload is not truncated"), Scan.bTruncated);
		TestEqual(TEXT("control: the in-prefix path was collected"), Scan.Paths.Num(), 1);
		TestTrue(TEXT("control: an in-prefix target is admitted"),
			CheckPathCoverage(Confined, Demand, Scan).bAllowed);
	}

	// Padding past the node budget must be reported, not silently swallowed.
	{
		const TSharedPtr<FJsonObject> Payload =
			Fields({ { TEXT("folderPath"), TEXT("/Game/TeamB/Secret") } });
		AddPadding(Payload, 5000);

		const FMcpPayloadPathScan Scan = CollectPayloadPaths(Payload, true);
		TestTrue(TEXT("a >4096-node payload reports a truncated scan"), Scan.bTruncated);

		const FMcpAuthorizationDecision Decision = CheckPathCoverage(Confined, Demand, Scan);
		TestFalse(TEXT("a truncated scan is refused, not admitted"), Decision.bAllowed);
		TestEqual(TEXT("refusal carries the path typed code"), Decision.ErrorCode,
			FString(TEXT("PATH_NOT_PERMITTED")));

		// An unrestricted principal was never subject to confinement, so the new
		// rule must not start refusing its large payloads.
		TestTrue(TEXT("an unrestricted principal is unaffected by truncation"),
			CheckPathCoverage(Unconfined, Demand, Scan).bAllowed);
	}

	// Nesting the real path below the depth bound is the same attack.
	{
		const TSharedPtr<FJsonObject> Deep =
			Nest(Fields({ { TEXT("folderPath"), TEXT("/Game/TeamB/Secret") } }), 12);
		const FMcpPayloadPathScan Scan = CollectPayloadPaths(Deep, true);
		TestTrue(TEXT("a value nested below the depth bound reports truncation"),
			Scan.bTruncated);
		TestFalse(TEXT("the over-deep payload is refused"),
			CheckPathCoverage(Confined, Demand, Scan).bAllowed);

		// POSITIVE CONTROL: nesting that stays inside the bound still resolves.
		const TSharedPtr<FJsonObject> Shallow =
			Nest(Fields({ { TEXT("folderPath"), TEXT("/Game/TeamA/Sub") } }), 2);
		const FMcpPayloadPathScan ShallowScan = CollectPayloadPaths(Shallow, true);
		TestFalse(TEXT("control: shallow nesting is not truncated"), ShallowScan.bTruncated);
		TestEqual(TEXT("control: the nested in-prefix path was collected"),
			ShallowScan.Paths.Num(), 1);
		TestTrue(TEXT("control: nested in-prefix target is admitted"),
			CheckPathCoverage(Confined, Demand, ShallowScan).bAllowed);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpPrequeueGateWriteTargetProofTest,
	"McpAutomationBridge.Foundation.PrequeueGate.WriteTargetProof",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPrequeueGateWriteTargetProofTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	if (!TestTrue(TEXT("capability store is ready"), FMcpCapabilityStore::Get().IsReady()))
	{
		return false;
	}
	FMcpPrincipalQuotaLedger::Get().Reset();

	const FMcpCapabilityPrincipal Confined =
		CoveragePrincipal({ EMcpCapabilityScope::Write }, { TEXT("/Game/TeamA") });
	const FMcpCapabilityPrincipal Unconfined =
		CoveragePrincipal({ EMcpCapabilityScope::Write }, {});

	auto Authorize = [](const FMcpCapabilityPrincipal& Principal, const TCHAR* Parent,
						 const TSharedPtr<FJsonObject>& Payload)
	{
		FMcpPrequeueRequest Request;
		Request.Principal = &Principal;
		Request.DispatchAction = Parent;
		Request.Payload = Payload;
		return McpPrequeueGate::Authorize(Request);
	};
	auto Demand = [](const TCHAR* Parent, const TCHAR* Action)
	{
		FMcpPrequeueRequest Request;
		Request.DispatchAction = Parent;
		Request.Payload = Fields({ { TEXT("subAction"), Action } });
		return McpPrequeueGate::ResolveDemand(Request);
	};

	// THE NARROWING, asserted directly. The rule fires only for a capability
	// whose own record declares somewhere to write. If this classification ever
	// flips, the cases below stop meaning what they claim.
	TestTrue(TEXT("create_behavior_tree declares a path parameter"),
		Demand(TEXT("manage_ai"), TEXT("create_behavior_tree")).bDeclaresPathParameter);
	TestTrue(TEXT("create_sublevel declares a path parameter"),
		Demand(TEXT("manage_level_structure"), TEXT("create_sublevel")).bDeclaresPathParameter);
	TestFalse(TEXT("console_command declares NO path parameter"),
		Demand(TEXT("system_control"), TEXT("console_command")).bDeclaresPathParameter);

	// PoC A — the asset name carries the folder, under a key no allowlist names,
	// and the declared savePath/path is omitted so the server default applies.
	{
		const FMcpAuthorizationDecision Decision = Authorize(Confined, TEXT("manage_ai"),
			Fields({ { TEXT("subAction"), TEXT("create_behavior_tree") },
					 { TEXT("name"), TEXT("TeamB/Secret") } }));
		TestFalse(TEXT("PoC A: a write with no provable target is refused"), Decision.bAllowed);
		TestEqual(TEXT("PoC A: refused with the path typed code"), Decision.ErrorCode,
			FString(TEXT("PATH_NOT_PERMITTED")));
	}

	// PoC B — the folder comes from the open editor world and is never in the
	// payload at all, so no key-based fix could ever reach it.
	{
		const FMcpAuthorizationDecision Decision = Authorize(Confined,
			TEXT("manage_level_structure"),
			Fields({ { TEXT("subAction"), TEXT("create_sublevel") },
					 { TEXT("sublevelName"), TEXT("Secret") } }));
		TestFalse(TEXT("PoC B: a sublevel write with no supplied path is refused"),
			Decision.bAllowed);
		TestEqual(TEXT("PoC B: refused with the path typed code"), Decision.ErrorCode,
			FString(TEXT("PATH_NOT_PERMITTED")));
	}

	// POSITIVE CONTROLS: supplying the declared path explicitly is exactly how a
	// confined principal is meant to work, and must still succeed.
	TestTrue(TEXT("control: an explicit in-prefix path is admitted"),
		Authorize(Confined, TEXT("manage_ai"),
			Fields({ { TEXT("subAction"), TEXT("create_behavior_tree") },
					 { TEXT("name"), TEXT("BT_Thing") },
					 { TEXT("path"), TEXT("/Game/TeamA/AI") } })).bAllowed);
	TestTrue(TEXT("control: an explicit in-prefix sublevel path is admitted"),
		Authorize(Confined, TEXT("manage_level_structure"),
			Fields({ { TEXT("subAction"), TEXT("create_sublevel") },
					 { TEXT("sublevelName"), TEXT("Sub01") },
					 { TEXT("sublevelPath"), TEXT("/Game/TeamA/Maps") } })).bAllowed);

	// THE NARROWING'S REASON: a legitimately pathless write must NOT regress.
	// console_command declares no path parameter, so a confined principal keeps
	// running it — this is live matrix case B2-0.
	TestTrue(TEXT("control: a pathless write capability is still admitted"),
		Authorize(Confined, TEXT("system_control"),
			Fields({ { TEXT("subAction"), TEXT("console_command") },
					 { TEXT("command"), TEXT("stat fps") } })).bAllowed);

	// An unrestricted principal is untouched by any of this.
	TestTrue(TEXT("control: an unrestricted principal keeps its previous behaviour"),
		Authorize(Unconfined, TEXT("manage_ai"),
			Fields({ { TEXT("subAction"), TEXT("create_behavior_tree") },
					 { TEXT("name"), TEXT("TeamB/Secret") } })).bAllowed);

	// An out-of-prefix path that IS supplied is still refused by containment,
	// so the new rule did not replace the old one.
	TestFalse(TEXT("an explicit out-of-prefix path is still refused"),
		Authorize(Confined, TEXT("manage_ai"),
			Fields({ { TEXT("subAction"), TEXT("create_behavior_tree") },
					 { TEXT("name"), TEXT("BT_Thing") },
					 { TEXT("path"), TEXT("/Game/TeamB/AI") } })).bAllowed);

	// The console-command scan fails closed on the same padding attack. This one
	// applies to every principal, because the Task 22 policy is not per-principal.
	{
		const TSharedPtr<FJsonObject> Payload =
			Fields({ { TEXT("subAction"), TEXT("console_command") },
					 { TEXT("command"), TEXT("stat fps") } });
		AddPadding(Payload, 5000);
		const FMcpAuthorizationDecision Decision =
			Authorize(Unconfined, TEXT("system_control"), Payload);
		TestFalse(TEXT("a payload too large to check for blocked commands is refused"),
			Decision.bAllowed);
		TestEqual(TEXT("refused with the command typed code"), Decision.ErrorCode,
			FString(TEXT("COMMAND_BLOCKED")));
	}

	FMcpPrincipalQuotaLedger::Get().Reset();
	return true;
}
#endif

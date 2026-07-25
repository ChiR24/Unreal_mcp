#include "Foundation/McpPrincipalQuota.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
FMcpCapabilityPrincipal QuotaPrincipal(const TCHAR* Identity, int32 RequestsPerMinute)
{
	FMcpCapabilityPrincipal Principal;
	Principal.Identity = Identity;
	Principal.Scopes = { EMcpCapabilityScope::Write };
	Principal.bAuthenticated = true;
	Principal.MaxRequestsPerMinute = RequestsPerMinute;
	return Principal;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpPrincipalQuotaLedgerTest,
	"McpAutomationBridge.Foundation.PrincipalQuota.Ledger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPrincipalQuotaLedgerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpPrincipalQuotaLedger& Ledger = FMcpPrincipalQuotaLedger::Get();
	Ledger.Reset();

	FString Reason;

	// An unconfigured limit is unlimited and never enters the ledger, so the
	// default loopback/legacy admin behaviour is unchanged.
	const FMcpCapabilityPrincipal Unlimited = QuotaPrincipal(TEXT("loopback"), 0);
	for (int32 Index = 0; Index < 50; ++Index)
	{
		TestTrue(TEXT("unlimited principal always passes"), Ledger.TryCharge(Unlimited, true, Reason));
	}
	TestEqual(TEXT("unlimited principal is never tracked"), Ledger.GetTrackedPrincipalCount(), 0);

	// Quota is keyed on the STABLE identity, so a reconnect (a fresh principal
	// value carrying the same identity) keeps spending the SAME window. This is
	// the reconnect bypass the per-socket rate limit could not close.
	const FMcpCapabilityPrincipal First = QuotaPrincipal(TEXT("scoped:limited"), 3);
	TestTrue(TEXT("1st"), Ledger.TryCharge(First, true, Reason));
	TestTrue(TEXT("2nd"), Ledger.TryCharge(First, true, Reason));

	const FMcpCapabilityPrincipal Reconnected = QuotaPrincipal(TEXT("scoped:limited"), 3);
	TestTrue(TEXT("3rd after reconnect"), Ledger.TryCharge(Reconnected, true, Reason));
	TestFalse(TEXT("4th is refused - reconnect did NOT reset the window"),
		Ledger.TryCharge(Reconnected, true, Reason));
	TestTrue(TEXT("refusal explains itself"), Reason.Contains(TEXT("quota")));
	TestFalse(TEXT("refusal never carries a token"), Reason.Contains(TEXT("secret")));

	// A refused charge must not consume budget, so the counter cannot run away.
	TestFalse(TEXT("still refused"), Ledger.TryCharge(Reconnected, true, Reason));

	// One principal exhausting its budget must not affect another.
	const FMcpCapabilityPrincipal Other = QuotaPrincipal(TEXT("scoped:other"), 3);
	TestTrue(TEXT("other principal is isolated"), Ledger.TryCharge(Other, true, Reason));

	// Bounded: a hostile client cannot grow the ledger without limit.
	Ledger.Reset();
	for (int32 Index = 0; Index < FMcpPrincipalQuotaLedger::MaxTrackedPrincipals + 64; ++Index)
	{
		const FMcpCapabilityPrincipal Churn =
			QuotaPrincipal(*FString::Printf(TEXT("scoped:churn%d"), Index), 10);
		Ledger.TryCharge(Churn, true, Reason);
	}
	TestTrue(TEXT("ledger stays bounded"),
		Ledger.GetTrackedPrincipalCount() <= FMcpPrincipalQuotaLedger::MaxTrackedPrincipals);

	Ledger.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpPrincipalToolCallQuotaTest,
	"McpAutomationBridge.Foundation.PrincipalQuota.ToolCallBudgetIsDistinct",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpPrincipalToolCallQuotaTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpPrincipalQuotaLedger& Ledger = FMcpPrincipalQuotaLedger::Get();
	Ledger.Reset();
	FString Reason;

	// MaxToolCallsPerMinute and MaxRequestsPerMinute are separate settings, not a
	// collapsed min(). Only a tool call spends the tool-call budget; discovery
	// traffic (bIsToolCall=false) spends the request budget alone.
	FMcpCapabilityPrincipal Principal = QuotaPrincipal(TEXT("scoped:toolcalls"), 100);
	Principal.MaxToolCallsPerMinute = 2;

	TestTrue(TEXT("1st tool call fits"), Ledger.TryCharge(Principal, true, Reason));
	TestTrue(TEXT("2nd tool call fits"), Ledger.TryCharge(Principal, true, Reason));
	TestFalse(TEXT("3rd tool call exceeds the tool-call budget"),
		Ledger.TryCharge(Principal, true, Reason));
	TestTrue(TEXT("the refusal names the tool-call budget, not the request budget"),
		Reason.Contains(TEXT("Tool-call quota")));

	// POSITIVE CONTROL: the generous request budget is untouched, so a discovery
	// request still passes after the tool-call budget is gone.
	TestTrue(TEXT("a discovery request still fits the request budget"),
		Ledger.TryCharge(Principal, false, Reason));

	// POSITIVE CONTROL: an unconfigured tool-call limit is unlimited.
	FMcpCapabilityPrincipal Wide = QuotaPrincipal(TEXT("scoped:widetoolcalls"), 100);
	Wide.MaxToolCallsPerMinute = 0;
	for (int32 Index = 0; Index < 20; ++Index)
	{
		TestTrue(TEXT("control: an unconfigured tool-call limit never refuses"),
			Ledger.TryCharge(Wide, true, Reason));
	}

	Ledger.Reset();
	return true;
}
#endif

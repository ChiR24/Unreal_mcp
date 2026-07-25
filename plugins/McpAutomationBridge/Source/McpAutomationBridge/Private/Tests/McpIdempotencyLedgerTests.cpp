#include "Foundation/McpIdempotencyLedger.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
const TCHAR* const IdemWriterPrincipal = TEXT("scoped:qawriter");
const TCHAR* const IdemImportCapability = TEXT("asset.import_asset");
const TCHAR* const IdemReceiptJson = TEXT("{\"success\":true,\"capabilityId\":\"asset.import_asset\"}");

EMcpIdempotencyOutcome BeginIdem(
	FMcpIdempotencyLedger& Ledger,
	const TCHAR* Principal,
	const TCHAR* Capability,
	const TCHAR* Key,
	const TCHAR* Fingerprint,
	FString& OutSlot,
	FString& OutReplay)
{
	return Ledger.Begin(Principal, Capability, Key, Fingerprint, OutSlot, OutReplay);
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpIdempotencyLedgerReplayTest,
	"McpAutomationBridge.Foundation.IdempotencyLedger.Replay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpIdempotencyLedgerReplayTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpIdempotencyLedger& Ledger = FMcpIdempotencyLedger::Get();
	Ledger.Reset();

	FString Slot;
	FString Replay;

	TestEqual(TEXT("first execution claims the slot"),
		BeginIdem(Ledger, IdemWriterPrincipal, IdemImportCapability, TEXT("k1"), TEXT("fp-a"), Slot, Replay),
		EMcpIdempotencyOutcome::First);

	// A duplicate arriving before the first finishes must NOT become a second
	// dispatch; that is the mutation this ledger exists to prevent.
	FString OtherSlot;
	TestEqual(TEXT("concurrent duplicate is in-flight"),
		BeginIdem(Ledger, IdemWriterPrincipal, IdemImportCapability, TEXT("k1"), TEXT("fp-a"), OtherSlot, Replay),
		EMcpIdempotencyOutcome::InFlight);

	Ledger.Complete(Slot, IdemReceiptJson);

	TestEqual(TEXT("identical retry replays"),
		BeginIdem(Ledger, IdemWriterPrincipal, IdemImportCapability, TEXT("k1"), TEXT("fp-a"), OtherSlot, Replay),
		EMcpIdempotencyOutcome::Replay);
	TestEqual(TEXT("the recorded receipt is returned verbatim"), Replay, FString(IdemReceiptJson));

	// An empty key opts out entirely, so a capability that never supplies one
	// keeps its existing behaviour and costs no ledger entry.
	Ledger.Reset();
	TestEqual(TEXT("an empty key disables dedup"),
		BeginIdem(Ledger, IdemWriterPrincipal, IdemImportCapability, TEXT(""), TEXT("fp-a"), Slot, Replay),
		EMcpIdempotencyOutcome::Disabled);
	TestEqual(TEXT("a disabled request stores nothing"), Ledger.GetEntryCount(), 0);

	Ledger.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpIdempotencyLedgerConflictTest,
	"McpAutomationBridge.Foundation.IdempotencyLedger.Conflict",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpIdempotencyLedgerConflictTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpIdempotencyLedger& Ledger = FMcpIdempotencyLedger::Get();
	Ledger.Reset();

	FString Slot;
	FString Replay;
	BeginIdem(Ledger, IdemWriterPrincipal, IdemImportCapability, TEXT("k1"), TEXT("fp-a"), Slot, Replay);
	Ledger.Complete(Slot, IdemReceiptJson);

	FString ClashSlot;
	FString ClashReplay;
	TestEqual(TEXT("a key reused with different params conflicts"),
		BeginIdem(Ledger, IdemWriterPrincipal, IdemImportCapability, TEXT("k1"), TEXT("fp-DIFFERENT"), ClashSlot, ClashReplay),
		EMcpIdempotencyOutcome::Conflict);
	// A caller that guessed another caller's key must not learn what it produced.
	TestTrue(TEXT("a conflict leaks no recorded receipt"), ClashReplay.IsEmpty());

	// Principal and capability are bound into the digest, so the same key is a
	// different slot for a different principal or a different capability.
	FString IsolatedSlot;
	FString IsolatedReplay;
	TestEqual(TEXT("another principal is isolated"),
		BeginIdem(Ledger, TEXT("scoped:other"), IdemImportCapability, TEXT("k1"), TEXT("fp-a"), IsolatedSlot, IsolatedReplay),
		EMcpIdempotencyOutcome::First);
	TestEqual(TEXT("another capability is isolated"),
		BeginIdem(Ledger, IdemWriterPrincipal, TEXT("asset.delete_asset"), TEXT("k1"), TEXT("fp-a"), IsolatedSlot, IsolatedReplay),
		EMcpIdempotencyOutcome::First);

	Ledger.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpIdempotencyLedgerFailureTest,
	"McpAutomationBridge.Foundation.IdempotencyLedger.FailuresAreNeverCached",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpIdempotencyLedgerFailureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpIdempotencyLedger& Ledger = FMcpIdempotencyLedger::Get();
	Ledger.Reset();

	FString Slot;
	FString Replay;
	BeginIdem(Ledger, IdemWriterPrincipal, IdemImportCapability, TEXT("k1"), TEXT("fp-a"), Slot, Replay);

	// A failed execution must leave nothing behind, or one transient bridge
	// error would be frozen into a replayed error for the whole TTL.
	Ledger.Abandon(Slot);
	TestEqual(TEXT("abandon drops the entry"), Ledger.GetEntryCount(), 0);

	FString RetrySlot;
	TestEqual(TEXT("the key stays retryable after a failure"),
		BeginIdem(Ledger, IdemWriterPrincipal, IdemImportCapability, TEXT("k1"), TEXT("fp-a"), RetrySlot, Replay),
		EMcpIdempotencyOutcome::First);

	Ledger.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpIdempotencyLedgerExpiryTest,
	"McpAutomationBridge.Foundation.IdempotencyLedger.TtlAndCap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpIdempotencyLedgerExpiryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpIdempotencyLedger& Ledger = FMcpIdempotencyLedger::Get();
	Ledger.Reset();

	double FakeNow = 1000.0;
	Ledger.SetClockForTests([&FakeNow]() { return FakeNow; });

	FString Slot;
	FString Replay;
	BeginIdem(Ledger, IdemWriterPrincipal, IdemImportCapability, TEXT("k1"), TEXT("fp-a"), Slot, Replay);
	Ledger.Complete(Slot, IdemReceiptJson);

	FakeNow += FMcpIdempotencyLedger::TtlSeconds - 1.0;
	FString Probe;
	TestEqual(TEXT("still replays just inside the TTL"),
		BeginIdem(Ledger, IdemWriterPrincipal, IdemImportCapability, TEXT("k1"), TEXT("fp-a"), Probe, Replay),
		EMcpIdempotencyOutcome::Replay);

	FakeNow += 2.0;
	TestEqual(TEXT("expires past the TTL"),
		BeginIdem(Ledger, IdemWriterPrincipal, IdemImportCapability, TEXT("k1"), TEXT("fp-a"), Probe, Replay),
		EMcpIdempotencyOutcome::First);

	// Bounded growth: the completed population never exceeds MaxEntries.
	Ledger.Reset();
	Ledger.SetClockForTests([&FakeNow]() { return FakeNow; });
	for (int32 Index = 0; Index < FMcpIdempotencyLedger::MaxEntries + 8; ++Index)
	{
		FString LoopSlot;
		FString LoopReplay;
		const FString Key = FString::Printf(TEXT("key-%d"), Index);
		BeginIdem(Ledger, IdemWriterPrincipal, IdemImportCapability, *Key, TEXT("fp"), LoopSlot, LoopReplay);
		Ledger.Complete(LoopSlot, IdemReceiptJson);
	}
	TestEqual(TEXT("the ledger stays bounded at the cap"),
		Ledger.GetEntryCount(), FMcpIdempotencyLedger::MaxEntries);

	// Oldest-completed-first: the earliest keys are the ones that went.
	FString EvictedSlot;
	FString EvictedReplay;
	TestEqual(TEXT("the oldest completed entry was evicted"),
		BeginIdem(Ledger, IdemWriterPrincipal, IdemImportCapability, TEXT("key-0"), TEXT("fp"), EvictedSlot, EvictedReplay),
		EMcpIdempotencyOutcome::First);
	Ledger.Abandon(EvictedSlot);

	FString SurvivorSlot;
	FString SurvivorReplay;
	const FString Newest = FString::Printf(TEXT("key-%d"), FMcpIdempotencyLedger::MaxEntries + 7);
	TestEqual(TEXT("the newest completed entry survived"),
		BeginIdem(Ledger, IdemWriterPrincipal, IdemImportCapability, *Newest, TEXT("fp"), SurvivorSlot, SurvivorReplay),
		EMcpIdempotencyOutcome::Replay);

	Ledger.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpIdempotencyLedgerSecrecyTest,
	"McpAutomationBridge.Foundation.IdempotencyLedger.KeyIsNeverRetained",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpIdempotencyLedgerSecrecyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// The slot is the only thing the ledger keeps, so proving the raw key does
	// not appear in it is what keeps a key out of logs and evidence files.
	const TCHAR* const SecretKey = TEXT("super-secret-key-value");
	FString Slot;
	TestTrue(TEXT("the digest is computed"),
		FMcpIdempotencyLedger::ComputeSlot(IdemWriterPrincipal, IdemImportCapability, SecretKey, Slot));
	TestFalse(TEXT("the slot never contains the raw key"), Slot.Contains(SecretKey));
	TestEqual(TEXT("the slot is a SHA-256 hex digest"), Slot.Len(), 64);

	FString Repeat;
	FMcpIdempotencyLedger::ComputeSlot(IdemWriterPrincipal, IdemImportCapability, SecretKey, Repeat);
	TestEqual(TEXT("the digest is stable"), Slot, Repeat);

	FString Different;
	FMcpIdempotencyLedger::ComputeSlot(TEXT("scoped:other"), IdemImportCapability, SecretKey, Different);
	TestNotEqual(TEXT("a different principal yields a different slot"), Slot, Different);

	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

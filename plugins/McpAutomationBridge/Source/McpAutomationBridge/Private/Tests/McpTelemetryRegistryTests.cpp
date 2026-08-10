#include "Foundation/McpTelemetryRegistry.h"
#include "Foundation/McpTelemetrySchema.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
// Fake clock shared by the tests below. Queue wait and duration are exact
// deltas from this value, so nothing here sleeps or samples the wall clock.
double GFakeClockSeconds = 0.0;

void InstallFakeClock(FMcpTelemetryRegistry& Registry, double StartSeconds)
{
	GFakeClockSeconds = StartSeconds;
	Registry.SetClock([]() { return GFakeClockSeconds; });
}

double SampleValue(const FString& Rendered, const FString& Prefix)
{
	TArray<FString> Lines;
	Rendered.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		if (Line.StartsWith(Prefix + TEXT(" "), ESearchCase::CaseSensitive))
		{
			return FCString::Atod(*Line.Mid(Prefix.Len() + 1));
		}
	}
	return -1.0;
}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpTelemetryRegistryTimingTest,
	"McpAutomationBridge.Foundation.TelemetryRegistry.Timing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpTelemetryRegistryTimingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpTelemetryRegistry& Registry = FMcpTelemetryRegistry::Get();
	Registry.Reset();
	InstallFakeClock(Registry, 1000.0);

	Registry.BeginRequest(TEXT("req-a"), TEXT("write"));
	GFakeClockSeconds = 1000.12;
	Registry.MarkDispatched(TEXT("req-a"));
	GFakeClockSeconds = 1000.5;
	Registry.EndRequest(TEXT("req-a"), TEXT("success"), FString());

	const FString Rendered = Registry.RenderPrometheus();
	const FString QueueSum = FString::Printf(TEXT("%s_sum{surface=\"native\",action_class=\"write\"}"),
		McpTelemetrySchema::MetricQueueWaitSeconds());
	const FString DurationSum = FString::Printf(TEXT("%s_sum{surface=\"native\",action_class=\"write\"}"),
		McpTelemetrySchema::MetricRequestDurationSeconds());

	TestTrue(TEXT("queue wait is the enqueue->dispatch delta"),
		FMath::IsNearlyEqual(SampleValue(Rendered, QueueSum), 0.12, 1e-4));
	TestTrue(TEXT("duration is the dispatch->terminal delta"),
		FMath::IsNearlyEqual(SampleValue(Rendered, DurationSum), 0.38, 1e-4));
	TestEqual(TEXT("terminal drops the in-flight entry"), Registry.InFlightCount(), 0);

	Registry.SetClock(nullptr);
	Registry.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpTelemetryRegistryPercentileTest,
	"McpAutomationBridge.Foundation.TelemetryRegistry.Percentiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpTelemetryRegistryPercentileTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpTelemetryRegistry& Registry = FMcpTelemetryRegistry::Get();
	Registry.Reset();
	InstallFakeClock(Registry, 0.0);

	for (int32 Index = 1; Index <= 10; ++Index)
	{
		FMcpTelemetryObservation Observation;
		Observation.ActionClass = TEXT("read");
		Observation.Outcome = TEXT("success");
		Observation.DurationSeconds = static_cast<double>(Index) / 100.0;
		Registry.ObserveRequest(Observation);
	}

	// Nearest rank: p50 -> 5th smallest, p95 -> 10th smallest.
	TestTrue(TEXT("p50 is the fifth sample"),
		FMath::IsNearlyEqual(Registry.QuantileSeconds(FMcpTelemetryRegistry::RequestFamily(), TEXT("read"), 0.5), 0.05, 1e-9));
	TestTrue(TEXT("p95 is the tenth sample"),
		FMath::IsNearlyEqual(Registry.QuantileSeconds(FMcpTelemetryRegistry::RequestFamily(), TEXT("read"), 0.95), 0.1, 1e-9));
	TestTrue(TEXT("an empty series reports no percentile rather than zero"),
		Registry.QuantileSeconds(FMcpTelemetryRegistry::RequestFamily(), TEXT("destructive"), 0.95) < 0.0);

	const FString Rendered = Registry.RenderPrometheus();
	const FString Count = FString::Printf(TEXT("%s_count{surface=\"native\",action_class=\"read\"}"),
		McpTelemetrySchema::MetricRequestDurationSeconds());
	TestTrue(TEXT("histogram counts every observation"),
		FMath::IsNearlyEqual(SampleValue(Rendered, Count), 10.0, 1e-9));

	Registry.SetClock(nullptr);
	Registry.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpTelemetryRegistryCardinalityTest,
	"McpAutomationBridge.Foundation.TelemetryRegistry.Cardinality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpTelemetryRegistryCardinalityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpTelemetryRegistry& Registry = FMcpTelemetryRegistry::Get();
	Registry.Reset();
	InstallFakeClock(Registry, 0.0);

	for (int32 Index = 0; Index < 500; ++Index)
	{
		FMcpTelemetryObservation Observation;
		Observation.ActionClass = FString::Printf(TEXT("/Game/Secret/Asset_%d"), Index);
		Observation.Outcome = TEXT("failure");
		Observation.FailureClass = FString::Printf(TEXT("Bearer sk-live-%d"), Index);
		Observation.DurationSeconds = 0.01;
		Registry.ObserveRequest(Observation);
	}

	const FString Rendered = Registry.RenderPrometheus();
	TestFalse(TEXT("a content path never reaches a metric label"), Rendered.Contains(TEXT("/Game/")));
	TestFalse(TEXT("a token never reaches a metric label"), Rendered.Contains(TEXT("sk-live")));
	TestTrue(TEXT("series stay bounded under a high-cardinality flood"), Registry.SeriesCount() < 64);

	Registry.BeginRequest(TEXT("req-3f1c-8a90"), TEXT("read"));
	Registry.EndRequest(TEXT("req-3f1c-8a90"), TEXT("success"), FString());
	TestFalse(TEXT("a request id never reaches a metric label"),
		Registry.RenderPrometheus().Contains(TEXT("req-3f1c")));

	Registry.SetClock(nullptr);
	Registry.Reset();
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

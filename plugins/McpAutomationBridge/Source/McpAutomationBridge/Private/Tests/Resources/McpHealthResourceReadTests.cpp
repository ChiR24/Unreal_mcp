#include "Foundation/McpTelemetryRegistry.h"
#include "Foundation/McpTelemetrySchema.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "MCP/Resources/McpResourceCatalog.h"
#include "MCP/Resources/McpResourceReadContent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// Task 47 follow-up: proves the native telemetry is CLIENT-READABLE.
//
// Before this, FMcpTelemetryRegistry accumulated real counters in production
// and RenderPrometheus produced the shared exposition format, but nothing
// served that text - RenderPrometheus was reached only by the test suite, so
// `ue://health` answered RESOURCE_UNAVAILABLE over the wire. These tests
// therefore drive the READ PATH, never the renderer directly.

namespace
{
double GHealthClockSeconds = 0.0;

void InstallHealthFakeClock(FMcpTelemetryRegistry& Registry, double StartSeconds)
{
	GHealthClockSeconds = StartSeconds;
	Registry.SetClock([]() { return GHealthClockSeconds; });
}

double SampleMetricValue(const FString& Rendered, const FString& Prefix)
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

// Reproduces EXACTLY what FMcpNativeTransport::HandlePrimitiveMethod does for a
// resources/read: Classify(), then BuildReadBody(). Calling RenderPrometheus()
// here would prove only that the renderer works - which was already true while
// no client could reach it.
FString ServeHealthResourceThroughReadPath(FAutomationTestBase& Test)
{
	const FString& Uri = McpResourceCatalog::HealthUri();
	const McpResourceRead::EReadKind Kind = McpResourceRead::Classify(Uri);
	Test.TestEqual(TEXT("the transport read classifier must serve ue://health, not refuse it"),
		static_cast<int32>(Kind), static_cast<int32>(McpResourceRead::EReadKind::SocketReadable));
	if (Kind != McpResourceRead::EReadKind::SocketReadable)
	{
		return FString();
	}
	return McpResourceRead::BuildReadBody(Uri, McpInitialResourceRevision).Text;
}

TSharedPtr<FJsonObject> ServedData(const FString& ServedBody)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ServedBody);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return nullptr;
	}
	const TSharedPtr<FJsonObject>* Data = nullptr;
	return Root->TryGetObjectField(TEXT("data"), Data) && Data != nullptr ? *Data : nullptr;
}

FString ExpositionFromServedBody(const FString& ServedBody)
{
	const TSharedPtr<FJsonObject> Data = ServedData(ServedBody);
	FString Exposition;
	if (Data.IsValid())
	{
		Data->TryGetStringField(TEXT("metricsExposition"), Exposition);
	}
	return Exposition;
}

bool ServedTotals(const FString& ServedBody, int32& OutRequests, int32& OutFailures)
{
	const TSharedPtr<FJsonObject> Data = ServedData(ServedBody);
	const TSharedPtr<FJsonObject>* Readiness = nullptr;
	const TSharedPtr<FJsonObject>* Diagnostics = nullptr;
	const TSharedPtr<FJsonObject>* Totals = nullptr;
	if (!Data.IsValid() ||
		!Data->TryGetObjectField(TEXT("readiness"), Readiness) || Readiness == nullptr ||
		!Data->TryGetObjectField(TEXT("diagnostics"), Diagnostics) || Diagnostics == nullptr ||
		!(*Diagnostics)->TryGetObjectField(TEXT("totals"), Totals) || Totals == nullptr)
	{
		return false;
	}
	return (*Totals)->TryGetNumberField(TEXT("requests"), OutRequests) &&
		(*Totals)->TryGetNumberField(TEXT("failures"), OutFailures);
}

// Cardinality is a security boundary, so this enumerates EVERY label value in
// the served text rather than spot-checking a few known secrets.
bool EveryServedLabelValueIsBounded(const FString& Exposition, FString& OutOffender)
{
	TSet<FString> Allowed;
	const TArray<const TArray<FString>*> BoundedSets = {
		&McpTelemetrySchema::SurfaceValues(),
		&McpTelemetrySchema::ActionClassValues(),
		&McpTelemetrySchema::OutcomeValues(),
		&McpTelemetrySchema::FailureClassValues(),
		&McpTelemetrySchema::ReadinessComponentValues(),
	};
	for (const TArray<FString>* BoundedSet : BoundedSets)
	{
		for (const FString& Value : *BoundedSet)
		{
			Allowed.Add(Value);
		}
	}

	int32 Cursor = 0;
	while (true)
	{
		const int32 Open = Exposition.Find(TEXT("=\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Cursor);
		if (Open == INDEX_NONE)
		{
			return true;
		}
		const int32 Start = Open + 2;
		const int32 Close = Exposition.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
		if (Close == INDEX_NONE)
		{
			return true;
		}
		const FString Value = Exposition.Mid(Start, Close - Start);
		Cursor = Close + 1;
		if (Allowed.Contains(Value) || Value == TEXT("+Inf") || Value.IsNumeric())
		{
			continue;
		}
		OutOffender = Value;
		return false;
	}
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpHealthResourceTelemetryServedTest,
	"McpAutomationBridge.MCP.Resources.HealthTelemetryServed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpHealthResourceTelemetryServedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpTelemetryRegistry& Registry = FMcpTelemetryRegistry::Get();
	Registry.Reset();
	InstallHealthFakeClock(Registry, 2000.0);

	Registry.BeginRequest(TEXT("req-served-1"), TEXT("write"));
	GHealthClockSeconds = 2000.05;
	Registry.MarkDispatched(TEXT("req-served-1"));
	GHealthClockSeconds = 2000.2;
	Registry.EndRequest(TEXT("req-served-1"), TEXT("failure"), TEXT("SCOPE_NOT_GRANTED"));

	const FString Served = ServeHealthResourceThroughReadPath(*this);
	TestFalse(TEXT("the health read path returns a body"), Served.IsEmpty());

	const TCHAR* MetricNames[] = {
		McpTelemetrySchema::MetricRequestDurationSeconds(),
		McpTelemetrySchema::MetricRequestDurationQuantileSeconds(),
		McpTelemetrySchema::MetricQueueWaitSeconds(),
		McpTelemetrySchema::MetricQueueWaitQuantileSeconds(),
		McpTelemetrySchema::MetricRequestsByClassTotal(),
		McpTelemetrySchema::MetricFailuresByClassTotal(),
		McpTelemetrySchema::MetricReadinessComponent(),
		McpTelemetrySchema::MetricReady(),
	};
	const FString Exposition = ExpositionFromServedBody(Served);
	for (const TCHAR* Name : MetricNames)
	{
		TestTrue(FString::Printf(TEXT("served exposition carries %s"), Name),
			Exposition.Contains(Name, ESearchCase::CaseSensitive));
	}

	int32 ServedRequests = -1;
	int32 ServedFailures = -1;
	TestTrue(TEXT("the served body carries readiness and anonymous aggregate diagnostics"),
		ServedTotals(Served, ServedRequests, ServedFailures));
	TestEqual(TEXT("the served aggregates count the real observation"), ServedRequests, 1);
	TestEqual(TEXT("the served aggregates count the real failure"), ServedFailures, 1);

	const FString QueueSum = FString::Printf(TEXT("%s_sum{surface=\"native\",action_class=\"write\"}"),
		McpTelemetrySchema::MetricQueueWaitSeconds());
	TestTrue(TEXT("the served exposition carries the real queue-wait sample"),
		FMath::IsNearlyEqual(SampleMetricValue(Exposition, QueueSum), 0.05, 1e-4));

	Registry.SetClock(nullptr);
	Registry.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FMcpHealthResourceTelemetryRedactionTest,
	"McpAutomationBridge.MCP.Resources.HealthTelemetryRedaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMcpHealthResourceTelemetryRedactionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FMcpTelemetryRegistry& Registry = FMcpTelemetryRegistry::Get();
	Registry.Reset();
	InstallHealthFakeClock(Registry, 0.0);

	for (int32 Index = 0; Index < 500; ++Index)
	{
		FMcpTelemetryObservation Observation;
		Observation.ActionClass = FString::Printf(TEXT("/Game/Secret/Levels/ClientPitch_%d"), Index);
		Observation.Outcome = TEXT("failure");
		Observation.FailureClass = FString::Printf(TEXT("Bearer sk-live-9f2a7c41-%d"), Index);
		Observation.DurationSeconds = 0.01;
		Registry.ObserveRequest(Observation);
	}
	Registry.BeginRequest(TEXT("req-3f1c-8a90-b7e2"), TEXT("manage_asset.import_asset"));
	Registry.EndRequest(TEXT("req-3f1c-8a90-b7e2"), TEXT("success"), FString());

	const FString Served = ServeHealthResourceThroughReadPath(*this);
	TestFalse(TEXT("a content path never reaches the served body"), Served.Contains(TEXT("/Game/")));
	TestFalse(TEXT("a token never reaches the served body"), Served.Contains(TEXT("sk-live")));
	TestFalse(TEXT("a capability id never reaches the served body"), Served.Contains(TEXT("manage_asset")));
	TestFalse(TEXT("a request id never reaches the served body"), Served.Contains(TEXT("req-3f1c")));

	FString Offender;
	const bool bBounded = EveryServedLabelValueIsBounded(ExpositionFromServedBody(Served), Offender);
	TestTrue(FString::Printf(TEXT("every served label value is bounded (offender: %s)"), *Offender), bBounded);
	TestTrue(TEXT("series stay bounded under a high-cardinality flood"), Registry.SeriesCount() < 64);

	Registry.SetClock(nullptr);
	Registry.Reset();
	return true;
}

#endif // WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

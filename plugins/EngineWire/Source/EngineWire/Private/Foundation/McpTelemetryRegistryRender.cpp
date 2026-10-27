#include "Foundation/McpTelemetryRegistry.h"

#include "Foundation/McpTelemetrySchema.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/ScopeLock.h"

// Task 47 Prometheus text exposition for the native registry, split from the
// accumulation translation unit to keep each file inside the plugin's 250
// pure-line ceiling. Label VALUES here can only come from the bounded schema
// sets - a request id, action name or content path is never formatted into a
// metric line.

namespace
{
// Every series this registry records comes from the native surface.
const TCHAR* const LocalSurface = TEXT("native");

FString FormatSeconds(double Value)
{
	return FString::SanitizeFloat(Value, 0);
}

// A negative quantile means "no samples retained", which must surface as JSON
// null. Reporting 0 there would read as a real, very fast series.
TSharedPtr<FJsonValue> SecondsOrNull(double Value)
{
	if (Value < 0.0)
	{
		return MakeShared<FJsonValueNull>();
	}
	return MakeShared<FJsonValueNumber>(Value);
}

template <typename KeyType, typename ValueType>
TArray<KeyType> SortedKeys(const TMap<KeyType, ValueType>& Map)
{
	TArray<KeyType> Keys;
	Map.GetKeys(Keys);
	Keys.Sort([](const KeyType& A, const KeyType& B) { return A.Key != B.Key ? A.Key < B.Key : A.Value < B.Value; });
	return Keys;
}
} // namespace

void FMcpTelemetryRegistry::RenderHistogramLocked(TArray<FString>& Lines, const FString& Family, const TCHAR* Name) const
{
	Lines.Add(FString::Printf(TEXT("# TYPE %s histogram"), Name));
	const TArray<double>& Bounds = McpTelemetrySchema::LatencyBucketUpperBoundsSeconds();
	for (const FSeriesKey& Key : SortedKeys(Histograms))
	{
		if (Key.Key != Family)
		{
			continue;
		}
		const FHistogramState& State = Histograms[Key];
		const FString Labels = FString::Printf(TEXT("%s=\"%s\",%s=\"%s\""),
			McpTelemetrySchema::LabelSurface(), LocalSurface,
			McpTelemetrySchema::LabelActionClass(), *Key.Value);

		int32 Cumulative = 0;
		for (int32 Index = 0; Index < Bounds.Num(); ++Index)
		{
			Cumulative += State.BucketCounts.IsValidIndex(Index) ? State.BucketCounts[Index] : 0;
			Lines.Add(FString::Printf(TEXT("%s_bucket{%s,%s=\"%s\"} %d"),
				Name, *Labels, McpTelemetrySchema::LabelLe(), *FormatSeconds(Bounds[Index]), Cumulative));
		}
		Lines.Add(FString::Printf(TEXT("%s_bucket{%s,%s=\"+Inf\"} %d"),
			Name, *Labels, McpTelemetrySchema::LabelLe(), State.Count));
		Lines.Add(FString::Printf(TEXT("%s_sum{%s} %s"), Name, *Labels, *FormatSeconds(State.SumSeconds)));
		Lines.Add(FString::Printf(TEXT("%s_count{%s} %d"), Name, *Labels, State.Count));
	}
}

void FMcpTelemetryRegistry::RenderQuantilesLocked(TArray<FString>& Lines, const FString& Family, const TCHAR* Name) const
{
	Lines.Add(FString::Printf(TEXT("# TYPE %s gauge"), Name));
	for (const FSeriesKey& Key : SortedKeys(Histograms))
	{
		if (Key.Key != Family)
		{
			continue;
		}
		for (const double Quantile : McpTelemetrySchema::Quantiles())
		{
			const double Value = NearestRank(Histograms[Key].Samples, Quantile);
			if (Value < 0.0)
			{
				continue;
			}
			Lines.Add(FString::Printf(TEXT("%s{%s=\"%s\",%s=\"%s\",%s=\"%s\"} %s"),
				Name,
				McpTelemetrySchema::LabelSurface(), LocalSurface,
				McpTelemetrySchema::LabelActionClass(), *Key.Value,
				McpTelemetrySchema::LabelQuantile(), *FormatSeconds(Quantile),
				*FormatSeconds(Value)));
		}
	}
}

void FMcpTelemetryRegistry::RenderCountersLocked(TArray<FString>& Lines, const TMap<FSeriesKey, int32>& Counters,
	const TCHAR* Name, const TCHAR* SecondLabel) const
{
	Lines.Add(FString::Printf(TEXT("# TYPE %s counter"), Name));
	for (const FSeriesKey& Key : SortedKeys(Counters))
	{
		Lines.Add(FString::Printf(TEXT("%s{%s=\"%s\",%s=\"%s\",%s=\"%s\"} %d"),
			Name,
			McpTelemetrySchema::LabelSurface(), LocalSurface,
			McpTelemetrySchema::LabelActionClass(), *Key.Key,
			SecondLabel, *Key.Value,
			Counters[Key]));
	}
}

FString FMcpTelemetryRegistry::RenderPrometheus(const FMcpTelemetryReadinessView* Readiness) const
{
	FScopeLock Lock(&Mutex);
	TArray<FString> Lines;

	RenderHistogramLocked(Lines, RequestFamily(), McpTelemetrySchema::MetricRequestDurationSeconds());
	RenderQuantilesLocked(Lines, RequestFamily(), McpTelemetrySchema::MetricRequestDurationQuantileSeconds());
	RenderHistogramLocked(Lines, QueueFamily(), McpTelemetrySchema::MetricQueueWaitSeconds());
	RenderQuantilesLocked(Lines, QueueFamily(), McpTelemetrySchema::MetricQueueWaitQuantileSeconds());
	RenderCountersLocked(Lines, RequestCounters, McpTelemetrySchema::MetricRequestsByClassTotal(), McpTelemetrySchema::LabelOutcome());
	RenderCountersLocked(Lines, FailureCounters, McpTelemetrySchema::MetricFailuresByClassTotal(), McpTelemetrySchema::LabelFailureClass());

	Lines.Add(FString::Printf(TEXT("# TYPE %s gauge"), McpTelemetrySchema::MetricReadinessComponent()));
	if (Readiness != nullptr)
	{
		for (const FString& Component : McpTelemetrySchema::ReadinessComponentValues())
		{
			const bool* Value = Readiness->Components.Find(Component);
			Lines.Add(FString::Printf(TEXT("%s{%s=\"%s\"} %d"),
				McpTelemetrySchema::MetricReadinessComponent(),
				McpTelemetrySchema::LabelComponent(), *Component,
				(Value != nullptr && *Value) ? 1 : 0));
		}
	}

	Lines.Add(FString::Printf(TEXT("# TYPE %s gauge"), McpTelemetrySchema::MetricReady()));
	if (Readiness != nullptr)
	{
		Lines.Add(FString::Printf(TEXT("%s %d"), McpTelemetrySchema::MetricReady(), Readiness->bReady ? 1 : 0));
	}

	return FString::Join(Lines, TEXT("\n")) + TEXT("\n");
}

double FMcpTelemetryRegistry::AggregateQuantileLocked(const FString& Family, double Quantile) const
{
	TArray<double> Samples;
	for (const TPair<FSeriesKey, FHistogramState>& Entry : Histograms)
	{
		if (Entry.Key.Key == Family)
		{
			Samples.Append(Entry.Value.Samples);
		}
	}
	return NearestRank(MoveTemp(Samples), Quantile);
}

TSharedRef<FJsonObject> FMcpTelemetryRegistry::SnapshotJson() const
{
	FScopeLock Lock(&Mutex);

	int32 TotalRequests = 0;
	int32 TotalFailures = 0;
	TMap<FString, int32> RequestsByClass;
	TMap<FString, int32> FailuresByClass;
	TMap<FString, int32> FailuresByFailureClass;
	for (const TPair<FSeriesKey, int32>& Entry : RequestCounters)
	{
		TotalRequests += Entry.Value;
		RequestsByClass.FindOrAdd(Entry.Key.Key) += Entry.Value;
	}
	for (const TPair<FSeriesKey, int32>& Entry : FailureCounters)
	{
		TotalFailures += Entry.Value;
		FailuresByClass.FindOrAdd(Entry.Key.Key) += Entry.Value;
		FailuresByFailureClass.FindOrAdd(Entry.Key.Value) += Entry.Value;
	}

	TArray<TSharedPtr<FJsonValue>> ByActionClass;
	for (const FString& ActionClass : McpTelemetrySchema::ActionClassValues())
	{
		const int32 Count = RequestsByClass.FindRef(ActionClass);
		if (Count <= 0)
		{
			continue;
		}
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("actionClass"), ActionClass);
		Entry->SetNumberField(TEXT("count"), Count);
		Entry->SetNumberField(TEXT("failures"), FailuresByClass.FindRef(ActionClass));
		Entry->SetField(TEXT("p50Seconds"), SecondsOrNull(QuantileLocked(RequestFamily(), ActionClass, 0.5)));
		Entry->SetField(TEXT("p95Seconds"), SecondsOrNull(QuantileLocked(RequestFamily(), ActionClass, 0.95)));
		ByActionClass.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TArray<TSharedPtr<FJsonValue>> ByFailureClass;
	for (const FString& FailureClass : McpTelemetrySchema::FailureClassValues())
	{
		const int32 Count = FailuresByFailureClass.FindRef(FailureClass);
		if (Count <= 0)
		{
			continue;
		}
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("failureClass"), FailureClass);
		Entry->SetNumberField(TEXT("count"), Count);
		ByFailureClass.Add(MakeShared<FJsonValueObject>(Entry));
	}

	auto Totals = MakeShared<FJsonObject>();
	Totals->SetNumberField(TEXT("requests"), TotalRequests);
	Totals->SetNumberField(TEXT("failures"), TotalFailures);

	auto QueueWait = MakeShared<FJsonObject>();
	QueueWait->SetField(TEXT("p50Seconds"), SecondsOrNull(AggregateQuantileLocked(QueueFamily(), 0.5)));
	QueueWait->SetField(TEXT("p95Seconds"), SecondsOrNull(AggregateQuantileLocked(QueueFamily(), 0.95)));

	auto Snapshot = MakeShared<FJsonObject>();
	Snapshot->SetObjectField(TEXT("totals"), Totals);
	Snapshot->SetArrayField(TEXT("byActionClass"), ByActionClass);
	Snapshot->SetArrayField(TEXT("byFailureClass"), ByFailureClass);
	Snapshot->SetObjectField(TEXT("queueWait"), QueueWait);
	return Snapshot;
}

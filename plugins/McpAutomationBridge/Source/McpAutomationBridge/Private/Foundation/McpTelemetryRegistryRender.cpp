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
} // namespace

void FMcpTelemetryRegistry::RenderHistogramLocked(TArray<FString>& Lines, const FString& Family, const TCHAR* Name) const
{
	Lines.Add(FString::Printf(TEXT("# TYPE %s histogram"), Name));
	const TArray<double>& Bounds = McpTelemetrySchema::LatencyBucketUpperBoundsSeconds();
	TArray<FString> Keys;
	Histograms.GetKeys(Keys);
	Keys.Sort();

	for (const FString& Key : Keys)
	{
		if (!Key.StartsWith(Family + TEXT(" "), ESearchCase::CaseSensitive))
		{
			continue;
		}
		const FHistogramState& State = Histograms[Key];
		TArray<FString> Parts;
		Key.ParseIntoArray(Parts, TEXT(" "), true);
		if (Parts.Num() < 3)
		{
			continue;
		}
		const FString Labels = FString::Printf(TEXT("%s=\"%s\",%s=\"%s\""),
			McpTelemetrySchema::LabelSurface(), *Parts[1],
			McpTelemetrySchema::LabelActionClass(), *Parts[2]);

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
	TArray<FString> Keys;
	Histograms.GetKeys(Keys);
	Keys.Sort();

	for (const FString& Key : Keys)
	{
		if (!Key.StartsWith(Family + TEXT(" "), ESearchCase::CaseSensitive))
		{
			continue;
		}
		TArray<FString> Parts;
		Key.ParseIntoArray(Parts, TEXT(" "), true);
		if (Parts.Num() < 3)
		{
			continue;
		}
		for (const double Quantile : McpTelemetrySchema::Quantiles())
		{
			const double Value = QuantileLocked(Family, Parts[2], Quantile);
			if (Value < 0.0)
			{
				continue;
			}
			Lines.Add(FString::Printf(TEXT("%s{%s=\"%s\",%s=\"%s\",%s=\"%s\"} %s"),
				Name,
				McpTelemetrySchema::LabelSurface(), *Parts[1],
				McpTelemetrySchema::LabelActionClass(), *Parts[2],
				McpTelemetrySchema::LabelQuantile(), *FormatSeconds(Quantile),
				*FormatSeconds(Value)));
		}
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

	Lines.Add(FString::Printf(TEXT("# TYPE %s counter"), McpTelemetrySchema::MetricRequestsByClassTotal()));
	TArray<FString> RequestKeys;
	RequestCounters.GetKeys(RequestKeys);
	RequestKeys.Sort();
	for (const FString& Key : RequestKeys)
	{
		TArray<FString> Parts;
		Key.ParseIntoArray(Parts, TEXT(" "), true);
		if (Parts.Num() < 3)
		{
			continue;
		}
		Lines.Add(FString::Printf(TEXT("%s{%s=\"%s\",%s=\"%s\",%s=\"%s\"} %d"),
			McpTelemetrySchema::MetricRequestsByClassTotal(),
			McpTelemetrySchema::LabelSurface(), *Parts[0],
			McpTelemetrySchema::LabelActionClass(), *Parts[1],
			McpTelemetrySchema::LabelOutcome(), *Parts[2],
			RequestCounters[Key]));
	}

	Lines.Add(FString::Printf(TEXT("# TYPE %s counter"), McpTelemetrySchema::MetricFailuresByClassTotal()));
	TArray<FString> FailureKeys;
	FailureCounters.GetKeys(FailureKeys);
	FailureKeys.Sort();
	for (const FString& Key : FailureKeys)
	{
		TArray<FString> Parts;
		Key.ParseIntoArray(Parts, TEXT(" "), true);
		if (Parts.Num() < 3)
		{
			continue;
		}
		Lines.Add(FString::Printf(TEXT("%s{%s=\"%s\",%s=\"%s\",%s=\"%s\"} %d"),
			McpTelemetrySchema::MetricFailuresByClassTotal(),
			McpTelemetrySchema::LabelSurface(), *Parts[0],
			McpTelemetrySchema::LabelActionClass(), *Parts[1],
			McpTelemetrySchema::LabelFailureClass(), *Parts[2],
			FailureCounters[Key]));
	}

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

int32 FMcpTelemetryRegistry::SumMatchingLocked(
	const TMap<FString, int32>& Counters, const FString& Value, int32 Position) const
{
	int32 Total = 0;
	for (const TPair<FString, int32>& Entry : Counters)
	{
		TArray<FString> Parts;
		Entry.Key.ParseIntoArray(Parts, TEXT(" "), true);
		if (Parts.IsValidIndex(Position) && Parts[Position] == Value)
		{
			Total += Entry.Value;
		}
	}
	return Total;
}

double FMcpTelemetryRegistry::AggregateQuantileLocked(const FString& Family, double Quantile) const
{
	TArray<double> Samples;
	for (const TPair<FString, FHistogramState>& Entry : Histograms)
	{
		if (Entry.Key.StartsWith(Family + TEXT(" "), ESearchCase::CaseSensitive))
		{
			Samples.Append(Entry.Value.Samples);
		}
	}
	if (Samples.Num() == 0)
	{
		return -1.0;
	}
	Samples.Sort();
	const int32 Rank = FMath::Clamp(
		FMath::CeilToInt(Quantile * static_cast<double>(Samples.Num())), 1, Samples.Num());
	return Samples[Rank - 1];
}

TSharedRef<FJsonObject> FMcpTelemetryRegistry::SnapshotJson() const
{
	FScopeLock Lock(&Mutex);

	int32 TotalRequests = 0;
	for (const TPair<FString, int32>& Entry : RequestCounters)
	{
		TotalRequests += Entry.Value;
	}
	int32 TotalFailures = 0;
	for (const TPair<FString, int32>& Entry : FailureCounters)
	{
		TotalFailures += Entry.Value;
	}

	TArray<TSharedPtr<FJsonValue>> ByActionClass;
	for (const FString& ActionClass : McpTelemetrySchema::ActionClassValues())
	{
		const int32 Count = SumMatchingLocked(RequestCounters, ActionClass, 1);
		if (Count <= 0)
		{
			continue;
		}
		auto Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("actionClass"), ActionClass);
		Entry->SetNumberField(TEXT("count"), Count);
		Entry->SetNumberField(TEXT("failures"), SumMatchingLocked(FailureCounters, ActionClass, 1));
		Entry->SetField(TEXT("p50Seconds"), SecondsOrNull(QuantileLocked(RequestFamily(), ActionClass, 0.5)));
		Entry->SetField(TEXT("p95Seconds"), SecondsOrNull(QuantileLocked(RequestFamily(), ActionClass, 0.95)));
		ByActionClass.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TArray<TSharedPtr<FJsonValue>> ByFailureClass;
	for (const FString& FailureClass : McpTelemetrySchema::FailureClassValues())
	{
		const int32 Count = SumMatchingLocked(FailureCounters, FailureClass, 2);
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

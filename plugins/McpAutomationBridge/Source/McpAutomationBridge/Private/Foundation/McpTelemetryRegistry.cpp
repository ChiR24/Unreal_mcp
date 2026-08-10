#include "Foundation/McpTelemetryRegistry.h"

#include "Foundation/McpTelemetrySchema.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"

namespace
{
double NonNegative(double Value)
{
	return (FMath::IsFinite(Value) && Value > 0.0) ? Value : 0.0;
}
} // namespace

FMcpTelemetryRegistry& FMcpTelemetryRegistry::Get()
{
	static FMcpTelemetryRegistry Registry;
	return Registry;
}

void FMcpTelemetryRegistry::SetClock(TFunction<double()> InClock)
{
	FScopeLock Lock(&Mutex);
	Clock = MoveTemp(InClock);
}

void FMcpTelemetryRegistry::Reset()
{
	FScopeLock Lock(&Mutex);
	Histograms.Reset();
	RequestCounters.Reset();
	FailureCounters.Reset();
	InFlight.Reset();
}

double FMcpTelemetryRegistry::Now() const
{
	return Clock ? Clock() : FPlatformTime::Seconds();
}

FString FMcpTelemetryRegistry::SeriesKey(const FString& Family, const FString& ActionClass) const
{
	return FString::Printf(TEXT("%s %s %s"), *Family, *LocalSurface, *McpTelemetrySchema::CoerceActionClass(ActionClass));
}

void FMcpTelemetryRegistry::ObserveHistogram(const FString& Family, const FString& ActionClass, double Seconds)
{
	const FString Key = SeriesKey(Family, ActionClass);
	FHistogramState& State = Histograms.FindOrAdd(Key);
	const TArray<double>& Bounds = McpTelemetrySchema::LatencyBucketUpperBoundsSeconds();
	if (State.BucketCounts.Num() != Bounds.Num())
	{
		State.BucketCounts.Init(0, Bounds.Num());
	}

	for (int32 Index = 0; Index < Bounds.Num(); ++Index)
	{
		if (Seconds <= Bounds[Index])
		{
			++State.BucketCounts[Index];
			break;
		}
	}

	State.SumSeconds += Seconds;
	State.Count += 1;
	State.Samples.Add(Seconds);
	if (State.Samples.Num() > SampleWindow)
	{
		State.Samples.RemoveAt(0, State.Samples.Num() - SampleWindow);
	}
}

void FMcpTelemetryRegistry::ObserveRequest(const FMcpTelemetryObservation& Observation)
{
	const FString ActionClass = McpTelemetrySchema::CoerceActionClass(Observation.ActionClass);
	const FString Outcome = McpTelemetrySchema::CoerceOutcome(Observation.Outcome);

	FScopeLock Lock(&Mutex);
	ObserveHistogram(RequestFamily(), ActionClass, NonNegative(Observation.DurationSeconds));
	if (Observation.QueueWaitSeconds >= 0.0)
	{
		ObserveHistogram(QueueFamily(), ActionClass, NonNegative(Observation.QueueWaitSeconds));
	}

	int32& RequestCount = RequestCounters.FindOrAdd(
		FString::Printf(TEXT("%s %s %s"), *LocalSurface, *ActionClass, *Outcome));
	++RequestCount;

	if (Outcome == TEXT("failure"))
	{
		const FString FailureClass = McpTelemetrySchema::CoerceFailureClass(Observation.FailureClass);
		int32& FailureCount = FailureCounters.FindOrAdd(
			FString::Printf(TEXT("%s %s %s"), *LocalSurface, *ActionClass, *FailureClass));
		++FailureCount;
	}
}

void FMcpTelemetryRegistry::BeginRequest(const FString& RequestId, const FString& ActionClass)
{
	if (RequestId.IsEmpty())
	{
		return;
	}

	const FString Coerced = McpTelemetrySchema::CoerceActionClass(ActionClass);

	FScopeLock Lock(&Mutex);
	if (FInFlightState* Existing = InFlight.Find(RequestId))
	{
		if (Existing->ActionClass == McpTelemetrySchema::UnknownValue())
		{
			Existing->ActionClass = Coerced;
		}
		return;
	}

	if (InFlight.Num() >= MaxInFlight)
	{
		// An unterminated request must not grow the map without bound. Dropping
		// the oldest loses one sample; keeping it would leak for the session.
		for (auto It = InFlight.CreateIterator(); It; ++It)
		{
			It.RemoveCurrent();
			break;
		}
	}

	FInFlightState State;
	State.ActionClass = Coerced;
	State.StartedAtSeconds = Now();
	InFlight.Add(RequestId, MoveTemp(State));
}

void FMcpTelemetryRegistry::MarkDispatched(const FString& RequestId)
{
	FScopeLock Lock(&Mutex);
	if (FInFlightState* State = InFlight.Find(RequestId))
	{
		State->DispatchedAtSeconds = Now();
	}
}

void FMcpTelemetryRegistry::EndRequest(const FString& RequestId, const FString& Outcome, const FString& FailureClass)
{
	FInFlightState State;
	{
		FScopeLock Lock(&Mutex);
		if (!InFlight.RemoveAndCopyValue(RequestId, State))
		{
			return;
		}
	}

	const double EndedAt = Now();
	const double DispatchedAt =
		State.DispatchedAtSeconds >= 0.0 ? State.DispatchedAtSeconds : State.StartedAtSeconds;

	FMcpTelemetryObservation Observation;
	Observation.ActionClass = State.ActionClass;
	Observation.Outcome = Outcome;
	Observation.FailureClass = FailureClass;
	Observation.DurationSeconds = NonNegative(EndedAt - DispatchedAt);
	Observation.QueueWaitSeconds = NonNegative(DispatchedAt - State.StartedAtSeconds);
	ObserveRequest(Observation);
}

double FMcpTelemetryRegistry::QuantileLocked(const FString& Family, const FString& ActionClass, double Quantile) const
{
	const FHistogramState* State = Histograms.Find(SeriesKey(Family, ActionClass));
	if (State == nullptr || State->Samples.Num() == 0)
	{
		return -1.0;
	}

	TArray<double> Sorted = State->Samples;
	Sorted.Sort();
	const int32 Rank = FMath::Clamp(
		FMath::CeilToInt(Quantile * static_cast<double>(Sorted.Num())), 1, Sorted.Num());
	return Sorted[Rank - 1];
}

double FMcpTelemetryRegistry::QuantileSeconds(const FString& Family, const FString& ActionClass, double Quantile) const
{
	FScopeLock Lock(&Mutex);
	return QuantileLocked(Family, ActionClass, Quantile);
}

int32 FMcpTelemetryRegistry::RetainedSampleCount(const FString& Family, const FString& ActionClass) const
{
	FScopeLock Lock(&Mutex);
	const FHistogramState* State = Histograms.Find(SeriesKey(Family, ActionClass));
	return State != nullptr ? State->Samples.Num() : 0;
}

int32 FMcpTelemetryRegistry::InFlightCount() const
{
	FScopeLock Lock(&Mutex);
	return InFlight.Num();
}

int32 FMcpTelemetryRegistry::SeriesCount() const
{
	FScopeLock Lock(&Mutex);
	return Histograms.Num() + RequestCounters.Num() + FailureCounters.Num();
}

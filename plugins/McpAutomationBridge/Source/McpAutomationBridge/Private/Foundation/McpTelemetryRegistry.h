#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"

class FJsonObject;

// Task 47 native counters/histograms. The native surface used to be log-only:
// telemetry was aggregated into a summary UE_LOG line and nothing could scrape
// it. This registry keeps the SAME aggregate as real counters, histogram buckets
// and bounded percentile samples, and renders them in the exposition format the
// TypeScript surface uses, with identical family and label names
// (`Foundation/McpTelemetrySchema.h`).
//
// Observation only: nothing reads these counters back to change routing, retry,
// scheduling or authorization behaviour.
//
// The clock is INJECTABLE so queue-wait and duration are exact under test rather
// than sampled from a sleep.

struct FMcpTelemetryObservation
{
	FString Surface;
	FString ActionClass;
	FString Outcome;
	FString FailureClass;
	double DurationSeconds = 0.0;
	/** Negative means "no queue hop measured for this request". */
	double QueueWaitSeconds = -1.0;
};

struct FMcpTelemetryReadinessView
{
	bool bReady = false;
	TMap<FString, bool> Components;
};

class FMcpTelemetryRegistry
{
public:
	/** Process-wide registry shared by the WebSocket bridge and native /mcp. */
	static FMcpTelemetryRegistry& Get();

	/** Seconds-resolution clock. Defaults to FPlatformTime::Seconds(). */
	void SetClock(TFunction<double()> InClock);
	void Reset();

	void ObserveRequest(const FMcpTelemetryObservation& Observation);

	/**
	 * RequestId is a MAP KEY only. It is never a label and never reaches
	 * exported text - that is what keeps request cardinality out of the metric.
	 *
	 * Idempotent on the START time: the enqueue instant is recorded by the FIRST
	 * call (the queue admission) so the later dispatch-time call can only refine
	 * the action class, never restart the queue-wait interval.
	 */
	void BeginRequest(const FString& RequestId, const FString& ActionClass);
	void MarkDispatched(const FString& RequestId);
	void EndRequest(const FString& RequestId, const FString& Outcome, const FString& FailureClass);

	/** Nearest-rank percentile over the retained window; negative when empty. */
	double QuantileSeconds(const FString& Family, const FString& ActionClass, double Quantile) const;
	int32 RetainedSampleCount(const FString& Family, const FString& ActionClass) const;
	int32 InFlightCount() const;
	int32 SeriesCount() const;

	FString RenderPrometheus(const FMcpTelemetryReadinessView* Readiness = nullptr) const;

	/**
	 * Anonymous aggregate snapshot, key-for-key the TypeScript TelemetrySnapshot
	 * (`src/services/telemetry-registry.ts`), so a client reading `ue://health`
	 * sees the same diagnostics on either transport. Carries only bounded label
	 * values and numbers - never a request id, capability id, path or message.
	 */
	TSharedRef<FJsonObject> SnapshotJson() const;

	static const TCHAR* RequestFamily() { return TEXT("request"); }
	static const TCHAR* QueueFamily() { return TEXT("queue"); }

private:
	struct FHistogramState
	{
		TArray<int32> BucketCounts;
		double SumSeconds = 0.0;
		int32 Count = 0;
		TArray<double> Samples;
	};

	struct FInFlightState
	{
		FString ActionClass;
		double StartedAtSeconds = 0.0;
		double DispatchedAtSeconds = -1.0;
	};

	double Now() const;
	FString SeriesKey(const FString& Family, const FString& ActionClass) const;
	void ObserveHistogram(const FString& Family, const FString& ActionClass, double Seconds);
	double QuantileLocked(const FString& Family, const FString& ActionClass, double Quantile) const;
	void RenderHistogramLocked(TArray<FString>& Lines, const FString& Family, const TCHAR* Name) const;
	void RenderQuantilesLocked(TArray<FString>& Lines, const FString& Family, const TCHAR* Name) const;
	int32 SumMatchingLocked(const TMap<FString, int32>& Counters, const FString& Value, int32 Position) const;
	double AggregateQuantileLocked(const FString& Family, double Quantile) const;

	mutable FCriticalSection Mutex;
	TFunction<double()> Clock;
	TMap<FString, FHistogramState> Histograms;
	TMap<FString, int32> RequestCounters;
	TMap<FString, int32> FailureCounters;
	TMap<FString, FInFlightState> InFlight;
	FString LocalSurface = TEXT("native");

	static constexpr int32 SampleWindow = 256;
	static constexpr int32 MaxInFlight = 1024;
};

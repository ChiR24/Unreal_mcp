#pragma once

#include "Foundation/McpTelemetryRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS
// The fake clock the telemetry tests drive: queue wait and duration are exact deltas from it, so nothing sleeps.
inline double& McpTelemetryTestClockSeconds()
{
	static double Seconds = 0.0;
	return Seconds;
}

inline void InstallTelemetryFakeClock(FMcpTelemetryRegistry& Registry, double StartSeconds)
{
	McpTelemetryTestClockSeconds() = StartSeconds;
	Registry.SetClock([]() { return McpTelemetryTestClockSeconds(); });
}

// The value on Rendered's exposition line for Prefix; -1 when there is none.
inline double SampleTelemetryValue(const FString& Rendered, const FString& Prefix)
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
#endif

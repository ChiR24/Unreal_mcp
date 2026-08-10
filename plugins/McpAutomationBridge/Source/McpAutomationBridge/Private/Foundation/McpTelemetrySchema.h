#pragma once

#include "CoreMinimal.h"

// Task 47 native mirror of the TypeScript telemetry schema
// (`src/services/telemetry-schema.ts`). Both surfaces export the SAME metric
// family names, label names, bounded label value sets, histogram bucket bounds
// and quantiles; `tests/unit/telemetry/telemetry-schema-parity.test.ts` reads this
// file as TEXT and fails on any drift, so a metric added on one surface without
// the other cannot ship.
//
// The MCP_TELEMETRY_SCHEMA_BEGIN/END markers delimit the mirrored regions and
// are part of the contract - do not remove or rename them.
//
// Cardinality is a SECURITY boundary. Every label value below comes from a
// closed set, and Coerce* maps anything unresolved to "unknown" rather than
// passing it through. Capability ids, asset/file paths, tokens, prompts, project
// content, session ids and request ids are never dimensions.
namespace McpTelemetrySchema
{
	// MCP_TELEMETRY_SCHEMA_BEGIN MetricNames
	inline const TCHAR* MetricRequestDurationSeconds() { return TEXT("unreal_mcp_request_duration_seconds"); }
	inline const TCHAR* MetricRequestDurationQuantileSeconds() { return TEXT("unreal_mcp_request_duration_quantile_seconds"); }
	inline const TCHAR* MetricQueueWaitSeconds() { return TEXT("unreal_mcp_queue_wait_seconds"); }
	inline const TCHAR* MetricQueueWaitQuantileSeconds() { return TEXT("unreal_mcp_queue_wait_quantile_seconds"); }
	inline const TCHAR* MetricRequestsByClassTotal() { return TEXT("unreal_mcp_requests_by_class_total"); }
	inline const TCHAR* MetricFailuresByClassTotal() { return TEXT("unreal_mcp_failures_by_class_total"); }
	inline const TCHAR* MetricReadinessComponent() { return TEXT("unreal_mcp_readiness_component"); }
	inline const TCHAR* MetricReady() { return TEXT("unreal_mcp_ready"); }
	// MCP_TELEMETRY_SCHEMA_END MetricNames

	// MCP_TELEMETRY_SCHEMA_BEGIN LabelNames
	inline const TCHAR* LabelSurface() { return TEXT("surface"); }
	inline const TCHAR* LabelActionClass() { return TEXT("action_class"); }
	inline const TCHAR* LabelOutcome() { return TEXT("outcome"); }
	inline const TCHAR* LabelFailureClass() { return TEXT("failure_class"); }
	inline const TCHAR* LabelComponent() { return TEXT("component"); }
	inline const TCHAR* LabelQuantile() { return TEXT("quantile"); }
	inline const TCHAR* LabelLe() { return TEXT("le"); }
	// MCP_TELEMETRY_SCHEMA_END LabelNames

	inline const TArray<FString>& SurfaceValues()
	{
		// MCP_TELEMETRY_SCHEMA_BEGIN SurfaceValues
		static const TArray<FString> Values = {
			TEXT("native"),
			TEXT("typescript"),
		};
		// MCP_TELEMETRY_SCHEMA_END SurfaceValues
		return Values;
	}

	inline const TArray<FString>& ActionClassValues()
	{
		// MCP_TELEMETRY_SCHEMA_BEGIN ActionClassValues
		static const TArray<FString> Values = {
			TEXT("admin"),
			TEXT("destructive"),
			TEXT("read"),
			TEXT("unknown"),
			TEXT("write"),
		};
		// MCP_TELEMETRY_SCHEMA_END ActionClassValues
		return Values;
	}

	inline const TArray<FString>& OutcomeValues()
	{
		// MCP_TELEMETRY_SCHEMA_BEGIN OutcomeValues
		static const TArray<FString> Values = {
			TEXT("failure"),
			TEXT("success"),
		};
		// MCP_TELEMETRY_SCHEMA_END OutcomeValues
		return Values;
	}

	inline const TArray<FString>& FailureClassValues()
	{
		// MCP_TELEMETRY_SCHEMA_BEGIN FailureClassValues
		static const TArray<FString> Values = {
			TEXT("command_blocked"),
			TEXT("consent_required"),
			TEXT("internal"),
			TEXT("path_not_permitted"),
			TEXT("project_not_permitted"),
			TEXT("quota_exceeded"),
			TEXT("scope_not_granted"),
			TEXT("timeout"),
			TEXT("transport"),
			TEXT("unknown"),
			TEXT("validation"),
		};
		// MCP_TELEMETRY_SCHEMA_END FailureClassValues
		return Values;
	}

	inline const TArray<FString>& ReadinessComponentValues()
	{
		// MCP_TELEMETRY_SCHEMA_BEGIN ReadinessComponentValues
		static const TArray<FString> Values = {
			TEXT("editor"),
			TEXT("registry"),
			TEXT("transport"),
		};
		// MCP_TELEMETRY_SCHEMA_END ReadinessComponentValues
		return Values;
	}

	inline const TArray<double>& LatencyBucketUpperBoundsSeconds()
	{
		// MCP_TELEMETRY_SCHEMA_BEGIN LatencyBuckets
		static const TArray<double> Values = {
			0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10, 30,
		};
		// MCP_TELEMETRY_SCHEMA_END LatencyBuckets
		return Values;
	}

	inline const TArray<double>& Quantiles()
	{
		// MCP_TELEMETRY_SCHEMA_BEGIN Quantiles
		static const TArray<double> Values = {
			0.5, 0.9, 0.95, 0.99,
		};
		// MCP_TELEMETRY_SCHEMA_END Quantiles
		return Values;
	}

	inline FString UnknownValue() { return TEXT("unknown"); }

	inline FString CoerceFromSet(const TArray<FString>& Allowed, const FString& Candidate, const FString& Fallback)
	{
		const FString Normalized = Candidate.TrimStartAndEnd().ToLower();
		return Allowed.Contains(Normalized) ? Normalized : Fallback;
	}

	inline FString CoerceSurface(const FString& Candidate)
	{
		return CoerceFromSet(SurfaceValues(), Candidate, TEXT("native"));
	}

	/** Unresolved input becomes "unknown"; it is never passed through as a label. */
	inline FString CoerceActionClass(const FString& Candidate)
	{
		return CoerceFromSet(ActionClassValues(), Candidate, UnknownValue());
	}

	inline FString CoerceOutcome(const FString& Candidate)
	{
		return CoerceFromSet(OutcomeValues(), Candidate, TEXT("success"));
	}

	/** Accepts the shared refusal codes in any case/dash form, else "unknown". */
	inline FString CoerceFailureClass(const FString& Candidate)
	{
		FString Normalized = Candidate.TrimStartAndEnd().ToLower();
		Normalized.ReplaceInline(TEXT("-"), TEXT("_"));
		return CoerceFromSet(FailureClassValues(), Normalized, UnknownValue());
	}
}

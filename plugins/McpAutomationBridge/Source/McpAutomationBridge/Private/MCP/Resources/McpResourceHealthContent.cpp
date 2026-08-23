#include "MCP/Resources/McpResourceHealthContent.h"

#include "MCP/Gateway/McpNativeGatewayCapabilityStore.h"
#include "Foundation/Diagnostics/McpDiagnosticsSnapshot.h"
#include "Foundation/McpReadinessState.h"
#include "Foundation/McpTelemetryRegistry.h"
#include "Foundation/McpTelemetrySchema.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
	const TCHAR* ReasonRegistryLoadFailed = TEXT("registry_load_failed");
	const TCHAR* ReasonRegistryEmpty = TEXT("registry_empty");
	const TCHAR* ReasonTransportDisconnected = TEXT("transport_disconnected");
	const TCHAR* ReasonEditorUnavailable = TEXT("editor_unavailable");

	struct FComponentProbe
	{
		bool bOk = false;
		FString Reason;
	};

	// Loading is not serving: a store that reports Ready but holds zero records
	// cannot answer a discovery call, so it is NOT ready. Mirrors the TypeScript
	// registry probe's registry_empty branch.
	FComponentProbe ProbeRegistry()
	{
		const FMcpCapabilityStore& Store = FMcpCapabilityStore::Get();
		if (!Store.IsReady())
		{
			return { false, ReasonRegistryLoadFailed };
		}
		if (Store.GetRecords().Num() == 0)
		{
			return { false, ReasonRegistryEmpty };
		}
		return { true, FString() };
	}

	FComponentProbe ProbeTransport()
	{
		return FMcpReadinessState::Get().IsTransportReady()
			? FComponentProbe{ true, FString() }
			: FComponentProbe{ false, ReasonTransportDisconnected };
	}

	FComponentProbe ProbeEditor()
	{
		return FMcpReadinessState::Get().IsEditorReady()
			? FComponentProbe{ true, FString() }
			: FComponentProbe{ false, ReasonEditorUnavailable };
	}

	FComponentProbe ProbeComponent(const FString& Component)
	{
		if (Component == TEXT("registry"))
		{
			return ProbeRegistry();
		}
		if (Component == TEXT("transport"))
		{
			return ProbeTransport();
		}
		return ProbeEditor();
	}
}  // namespace

namespace McpResourceHealth
{
	TSharedRef<FJsonObject> BuildHealthData()
	{
		FMcpTelemetryReadinessView View;
		auto Components = MakeShared<FJsonObject>();
		TArray<TSharedPtr<FJsonValue>> NotReady;
		bool bReady = true;

		for (const FString& Component : McpTelemetrySchema::ReadinessComponentValues())
		{
			const FComponentProbe Probe = ProbeComponent(Component);
			View.Components.Add(Component, Probe.bOk);
			Components->SetBoolField(Component, Probe.bOk);
			if (!Probe.bOk)
			{
				bReady = false;
				NotReady.Add(MakeShared<FJsonValueString>(Probe.Reason));
			}
		}
		View.bReady = bReady;

		auto Readiness = MakeShared<FJsonObject>();
		Readiness->SetBoolField(TEXT("ready"), bReady);
		Readiness->SetObjectField(TEXT("components"), Components);
		Readiness->SetArrayField(TEXT("notReady"), NotReady);

		const FMcpTelemetryRegistry& Registry = FMcpTelemetryRegistry::Get();
		auto Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("surface"), McpTelemetrySchema::CoerceSurface(TEXT("native")));
		Data->SetObjectField(TEXT("readiness"), Readiness);
		Data->SetObjectField(TEXT("diagnostics"), Registry.SnapshotJson());
		// NF-6: identical null-when-empty projection to the automation-bridge
		// presenter (McpResourceBridgeContent.cpp) - cross-transport parity with
		// the TS reader's null for a missing previous FILE.
		const TSharedRef<FJsonObject> PreviousSummary = FMcpDiagnosticsSnapshot::Get().PreviousSummaryJson();
		Data->SetField(TEXT("previousSession"), PreviousSummary->Values.Num() == 0
			? TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>())
			: TSharedPtr<FJsonValue>(MakeShared<FJsonValueObject>(PreviousSummary)));
		Data->SetObjectField(TEXT("currentSession"), FMcpDiagnosticsSnapshot::Get().CurrentSummaryJson());
		Data->SetStringField(TEXT("metricsExposition"), Registry.RenderPrometheus(&View));
		return Data;
	}
}  // namespace McpResourceHealth

#include "MCP/Resources/McpResourceBridgeContent.h"

#include "EngineWireSettings.h"
#include "Foundation/Diagnostics/McpDiagnosticsSnapshot.h"
#include "Foundation/McpReadinessState.h"
#include "Foundation/McpTelemetryRegistry.h"
#include "Dom/JsonValue.h"

namespace
{
	int32 FirstListenPort(const UEngineWireSettings* Settings)
	{
		if (!Settings)
		{
			return 0;
		}
		FString First = Settings->ListenPorts;
		int32 Comma = INDEX_NONE;
		if (First.FindChar(TEXT(','), Comma))
		{
			First = First.Left(Comma);
		}
		return FCString::Atoi(*First.TrimStartAndEnd());
	}
}  // namespace

namespace McpResourceBridge
{
	TSharedRef<FJsonObject> BuildAutomationBridgeData()
	{
		const UEngineWireSettings* Settings = GetDefault<UEngineWireSettings>();
		const FMcpReadinessState& Readiness = FMcpReadinessState::Get();

		auto Summary = MakeShared<FJsonObject>();
		Summary->SetBoolField(TEXT("enabled"), Settings ? Settings->bAlwaysListen : false);
		Summary->SetBoolField(TEXT("connected"), Readiness.IsEditorReady());
		Summary->SetStringField(TEXT("host"), Settings ? Settings->ListenHost : FString());
		Summary->SetNumberField(TEXT("port"), FirstListenPort(Settings));
		Summary->SetBoolField(TEXT("capabilityTokenRequired"),
			Settings ? Settings->bRequireCapabilityToken : false);
		Summary->SetNumberField(TEXT("pendingRequests"),
			FMcpTelemetryRegistry::Get().InFlightCount());

		// The four connection timestamps, the last disconnect and the last two
		// failures are a bridge CLIENT's to track: it owns
		// the socket lifecycle it is reporting on. The plugin is the SERVER here
		// and holds no equivalent history, so these are reported as null rather
		// than back-filled from something that would merely look similar. The keys
		// stay, so a client parses one shape.
		auto Timestamps = MakeShared<FJsonObject>();
		for (const TCHAR* Key : { TEXT("connectedAt"), TEXT("lastHandshakeAt"),
			TEXT("lastMessageAt"), TEXT("lastRequestSentAt") })
		{
			Timestamps->SetField(Key, MakeShared<FJsonValueNull>());
		}

		auto Data = MakeShared<FJsonObject>();
		Data->SetObjectField(TEXT("summary"), Summary);
		Data->SetObjectField(TEXT("timestamps"), Timestamps);
		Data->SetField(TEXT("lastDisconnect"), MakeShared<FJsonValueNull>());
		Data->SetField(TEXT("lastHandshakeFailure"), MakeShared<FJsonValueNull>());
		Data->SetField(TEXT("lastError"), MakeShared<FJsonValueNull>());
		Data->SetBoolField(TEXT("listening"), Readiness.IsTransportReady());
		FMcpDiagnosticsSnapshot::Get().SetSessionFields(Data);
		return Data;
	}
}  // namespace McpResourceBridge

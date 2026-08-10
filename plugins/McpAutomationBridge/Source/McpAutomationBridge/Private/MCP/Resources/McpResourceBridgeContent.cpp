#include "MCP/Resources/McpResourceBridgeContent.h"

#include "McpAutomationBridgeSettings.h"
#include "Foundation/McpReadinessState.h"
#include "Foundation/McpTelemetryRegistry.h"
#include "Dom/JsonValue.h"
#include "Misc/EngineVersion.h"

namespace
{
	int32 FirstListenPort(const UMcpAutomationBridgeSettings* Settings)
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
	TSharedRef<FJsonObject> BuildEngineVersionData()
	{
		const FEngineVersion& Version = FEngineVersion::Current();
		const int32 Major = static_cast<int32>(Version.GetMajor());
		const int32 Minor = static_cast<int32>(Version.GetMinor());
		const int32 Patch = static_cast<int32>(Version.GetPatch());
		auto Data = MakeShared<FJsonObject>();
		Data->SetStringField(TEXT("version"),
			FString::Printf(TEXT("%d.%d.%d"), Major, Minor, Patch));
		Data->SetNumberField(TEXT("major"), Major);
		Data->SetNumberField(TEXT("minor"), Minor);
		Data->SetNumberField(TEXT("patch"), Patch);
		Data->SetBoolField(TEXT("isUE56OrAbove"), Major > 5 || (Major == 5 && Minor >= 6));
		return Data;
	}

	TSharedRef<FJsonObject> BuildAutomationBridgeData()
	{
		const UMcpAutomationBridgeSettings* Settings = GetDefault<UMcpAutomationBridgeSettings>();
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
		// failures are tracked only by the TypeScript bridge CLIENT, which owns
		// the socket lifecycle it is reporting on. The plugin is the SERVER here
		// and holds no equivalent history, so these are reported as null rather
		// than back-filled from something that would merely look similar. The key
		// set stays identical to the TypeScript body so a client can parse one
		// shape on both transports.
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
		return Data;
	}
}  // namespace McpResourceBridge

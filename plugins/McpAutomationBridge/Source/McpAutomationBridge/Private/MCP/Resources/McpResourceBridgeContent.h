// McpResourceBridgeContent.h
// Socket-thread read bodies for `ue://version` and `ue://automation-bridge`.
//
// Neither is editor state, which is why both belong here rather than behind the
// game thread. `ue://version` is FEngineVersion::Current() - a process constant
// that McpResourceReadContent already reads on this thread for `ue://project`.
// `ue://automation-bridge` is transport status: the plugin's own settings, the
// atomic readiness flags, and the telemetry in-flight counter. The TypeScript
// handler for that uri likewise does no editor round-trip (it never calls
// ensureConnected), so serving it here is a parity fix, not a new capability.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpResourceBridge
{
	TSharedRef<FJsonObject> BuildEngineVersionData();

	TSharedRef<FJsonObject> BuildAutomationBridgeData();
}

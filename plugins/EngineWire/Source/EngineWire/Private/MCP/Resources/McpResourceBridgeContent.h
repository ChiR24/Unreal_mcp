// McpResourceBridgeContent.h
// Socket-thread read body for ue://automation-bridge: transport status (the
// plugin's settings, readiness flags, in-flight counter), never editor state.
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace McpResourceBridge
{
	TSharedRef<FJsonObject> BuildAutomationBridgeData();
}

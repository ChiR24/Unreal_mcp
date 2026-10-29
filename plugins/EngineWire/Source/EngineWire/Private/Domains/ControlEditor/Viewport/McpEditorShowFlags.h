// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

// control_editor configure_viewport setting show_flag: the level viewport's Show menu by name (Grid, Collision, Bounds,
// Navigation, Fog...), as the engine's show flags, each set on or off and read back.
namespace McpEditorShowFlags
{
bool Handle(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket);
}

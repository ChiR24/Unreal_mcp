// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Templates/Function.h"

class AActor;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

// control_actor edit_component edit flicker: a placed light that flickers, through a light function material (the
// engine's way to vary a light with no logic), so a native PointLight, SpotLight or RectLight takes it as well as a
// Blueprint's light. /Game/Materials/Lights/MLF_Flicker is made once; each speed and depth pair is a material instance
// of it (MI_LightFlicker_S<speed x 10>_D<depth x 100>), made once and reused.
namespace McpLightFlicker
{
bool Handle(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket, TFunctionRef<AActor*(const FString&)> Find);
}

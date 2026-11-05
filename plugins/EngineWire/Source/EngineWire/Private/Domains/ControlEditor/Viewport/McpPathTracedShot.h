// control_editor screenshot pathTraced: the level viewport drawn by the path tracer to the samples asked, denoised, then
// read back as a screenshot is (McpPathTracedShot.cpp).

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

namespace McpPathTracedShot
{
/** True when Payload asks for a path-traced shot: it was refused (reply sent), or it is under way and answers later. */
bool Handle(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket);
}

// control_editor set_selected_actors: which actors the editor has selected (the Outliner and the Details panel follow them).

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;

namespace McpEditorSelection
{
bool Handle(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket);
}

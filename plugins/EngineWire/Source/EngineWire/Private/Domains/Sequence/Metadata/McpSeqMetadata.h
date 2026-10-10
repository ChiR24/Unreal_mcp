#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class UObject;

namespace McpSequenceMetadata {
// Package-metadata key/value pairs of an asset as a JSON object (sorted by key).
TSharedPtr<FJsonObject> BuildMetadataObject(UObject *Asset);

bool HandleGetMetadata(UEngineWireSubsystem *Subsystem,
                       const FString &RequestId,
                       const TSharedPtr<FJsonObject> &Payload,
                       TSharedPtr<FMcpBridgeWebSocket> Socket);
}

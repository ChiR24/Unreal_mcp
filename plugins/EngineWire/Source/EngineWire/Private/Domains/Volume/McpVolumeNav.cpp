#include "Domains/Volume/McpVolumeActionDecls.h"

#include "Dom/JsonObject.h"
#include "GameFramework/CameraBlockingVolume.h"
#include "EngineWireSubsystem.h"
#include "Domains/Volume/McpVolumeGeometry.h"
#include "Domains/Volume/McpVolumeRequestParsing.h"
#include "Domains/Volume/McpVolumeResponses.h"
#include "Domains/Volume/McpVolumeWorldResolve.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavModifierVolume.h"

namespace McpVolumeHandlers
{
bool HandleCreateNavMeshBoundsVolume(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return VolumeHelpers::CreateBoxVolume<ANavMeshBoundsVolume>(Subsystem, RequestId, Payload, Socket, FVector(2000.0f, 2000.0f, 500.0f));
}

bool HandleCreateNavModifierVolume(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return VolumeHelpers::CreateBoxVolume<ANavModifierVolume>(Subsystem, RequestId, Payload, Socket, FVector(500.0f, 500.0f, 200.0f));
}

bool HandleCreateCameraBlockingVolume(UEngineWireSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return VolumeHelpers::CreateBoxVolume<ACameraBlockingVolume>(Subsystem, RequestId, Payload, Socket, FVector(200.0f, 200.0f, 200.0f));
}
}

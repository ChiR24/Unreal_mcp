#include "Domains/Volume/McpAutomationBridge_VolumeActionDeclarations.h"

#include "Dom/JsonObject.h"
#include "GameFramework/CameraBlockingVolume.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Domains/Volume/McpAutomationBridge_VolumeGeometry.h"
#include "Domains/Volume/McpAutomationBridge_VolumeRequestParsing.h"
#include "Domains/Volume/McpAutomationBridge_VolumeResponses.h"
#include "Domains/Volume/McpAutomationBridge_VolumeWorldResolution.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavModifierVolume.h"

namespace McpVolumeHandlers
{
bool HandleCreateNavMeshBoundsVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return VolumeHelpers::CreateBoxVolume<ANavMeshBoundsVolume>(Subsystem, RequestId, Payload, Socket, FVector(2000.0f, 2000.0f, 500.0f));
}

bool HandleCreateNavModifierVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return VolumeHelpers::CreateBoxVolume<ANavModifierVolume>(Subsystem, RequestId, Payload, Socket, FVector(500.0f, 500.0f, 200.0f));
}

bool HandleCreateCameraBlockingVolume(UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    return VolumeHelpers::CreateBoxVolume<ACameraBlockingVolume>(Subsystem, RequestId, Payload, Socket, FVector(200.0f, 200.0f, 200.0f));
}
}

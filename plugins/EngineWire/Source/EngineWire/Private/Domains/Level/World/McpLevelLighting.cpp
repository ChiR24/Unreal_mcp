#include "Domains/Level/McpLevelActions.h"

#include "Domains/Lighting/McpLightingPrivate.h"
#include "Domains/Level/World/McpLevelWorldAccess.h"
#include "Editor.h"
#include "EditorBuildUtils.h"
#include "Engine/Level.h"
#include "Engine/World.h"

namespace McpLevelHandlers {
bool HandleBuildLightingAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return McpLightingHandlers::HandleBuildLighting(Subsystem, RequestId, Payload, RequestingSocket);
}

bool HandleSpawnLightAction(UEngineWireSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return McpLightingHandlers::HandleSpawnLight(Subsystem, RequestId, Payload, RequestingSocket);
}
}

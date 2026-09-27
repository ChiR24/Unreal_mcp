#include "Domains/Level/McpAutomationBridge_LevelHandlersActions.h"

#include "Domains/Lighting/McpAutomationBridge_LightingHandlersPrivate.h"
#include "Domains/Level/World/McpAutomationBridge_LevelHandlersWorldAccess.h"
#include "Editor.h"
#include "EditorBuildUtils.h"
#include "Engine/Level.h"
#include "Engine/World.h"

namespace McpLevelHandlers {
bool HandleBuildLightingAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return McpLightingHandlers::HandleBuildLighting(Subsystem, RequestId, Payload, RequestingSocket);
}

bool HandleSpawnLightAction(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return McpLightingHandlers::HandleSpawnLight(Subsystem, RequestId, Payload, RequestingSocket);
}
}

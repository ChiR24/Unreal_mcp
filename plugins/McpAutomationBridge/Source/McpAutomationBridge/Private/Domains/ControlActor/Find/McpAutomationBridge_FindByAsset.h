// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;

namespace McpFindByAsset
{
// control_actor find, findBy=mesh: the actors of the level (the Play In Editor world while a session runs) with a
// StaticMeshComponent, an instanced or foliage one too, that draws the mesh meshPath, each row naming those components.
bool HandleFindByMesh(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId,
                      const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);

// control_actor find, findBy=material: the actors with a component that uses the material materialPath in any slot, as
// an override or as the default of its mesh, each row naming the components and the slots.
bool HandleFindByMaterial(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId,
                          const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
}

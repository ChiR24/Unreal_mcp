// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "McpAutomationBridgeSubsystem.h"

#include "Dom/JsonObject.h"

namespace McpMeshMaterials
{
// process_asset process=mesh_materials (set_mesh_materials): assign materials to the slots of a static or skeletal
// mesh ASSET, by slot index or slot name, so every placement of the mesh shows them. The reply lists what each slot
// holds afterwards and every entry that was refused.
bool HandleSetMeshMaterials(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
}

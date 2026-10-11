// Copyright (c) 2024 EngineWire Contributors

#pragma once

#include "EngineWireSubsystem.h"

#include "Dom/JsonObject.h"

namespace McpMeshMaterials
{
// process_asset process=mesh_materials (set_mesh_materials): assign materials to the slots of a static or skeletal
// mesh ASSET, by slot index or slot name, so every placement of the mesh shows them. The reply lists what each slot
// holds afterwards and every entry that was refused.
bool HandleSetMeshMaterials(UEngineWireSubsystem* Bridge, const FString& RequestId,
                            const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);

// The same materials entries on every mesh of assetPaths, by slot name (McpAssetMeshMatsMany.cpp).
bool SetOnManyMeshes(UEngineWireSubsystem* Bridge, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                     const TArray<TSharedPtr<FJsonValue>>& Paths, const TArray<TSharedPtr<FJsonValue>>& Entries, bool bSave);
}

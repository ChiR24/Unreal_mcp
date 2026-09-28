// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "McpAutomationBridgeSubsystem.h"

#include "Dom/JsonObject.h"
#include "IAssetTools.h"

namespace McpAssetRename
{
// IAssetTools::RenameAssets asks OkCancel before it renames an asset that a native class default
// object still points at: a config setting (GameMapsSettings.GameDefaultMap, the Enhanced Input default
// mapping contexts) or the level editor's saved camera per map (LevelEditorViewportSettings.EditorViews,
// an entry for every map ever opened). An unattended editor answers Cancel, so the whole rename was
// dropped without a reason. This points those references at the new paths first, renames, saves the
// config files it changed, and puts everything back when the rename fails. A hard object reference
// from a class default cannot be moved; it is named under blockingReferences.
bool RenameWithSettingsFollow(const TArray<FAssetRenameData>& RenameData, const TSharedPtr<FJsonObject>& Report,
                              FString& OutFailure);

// Every redirector under Folder: its referencers are resaved against the target, then it is deleted.
void FixupRedirectorsIn(const FString& Folder, int32& OutFound, int32& OutFixed, bool bCheckoutFiles = false);

// move/rename with a folder as sourcePath: every asset under it moves, keeping the sub-folder layout.
bool HandleMoveFolder(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SourceFolder,
                      const FString& DestinationFolder, TSharedPtr<FMcpBridgeWebSocket> Socket);

// maintain_content refresh_blueprints: refresh every node, compile and save each Blueprint.
bool HandleRefreshBlueprints(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId,
                             const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);
}

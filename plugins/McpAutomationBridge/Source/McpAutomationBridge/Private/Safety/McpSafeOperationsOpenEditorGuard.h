#pragma once

#include "Safety/McpSafeOperationsAssetEditorSubsystem.h"

#if WITH_EDITOR
#include "Editor.h"
#include "UObject/Object.h"

/**
 * Open-asset-editor write guard.
 *
 * An open asset editor holds its own in-memory copy of the asset — the material
 * editor in particular edits a preview duplicate and writes it back over the
 * original when it closes. An automation write that lands on the on-disk object
 * while that editor is open therefore reports success, is visibly applied, and
 * is then silently discarded the moment the user closes the tab. That failure is
 * invisible at the call site and looks exactly like the handler lying about what
 * it did.
 *
 * Callers that mutate an asset directly should refuse rather than take the write,
 * so the caller learns the truth at the point of the call.
 */
namespace McpSafeOperations
{
/** True when an asset editor is currently open for this asset. */
inline bool IsAssetEditorOpen(UObject* Asset)
{
#if MCP_HAS_ASSET_EDITOR_SUBSYSTEM
	if (Asset == nullptr || GEditor == nullptr)
	{
		return false;
	}
	UAssetEditorSubsystem* AssetEditorSubsystem =
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (AssetEditorSubsystem == nullptr)
	{
		return false;
	}
	return AssetEditorSubsystem->FindEditorForAsset(Asset, /*bFocusIfOpen*/ false) != nullptr;
#else
	(void)Asset;
	return false;
#endif
}

/** Message explaining why the write was refused, naming the asset. */
inline FString OpenAssetEditorRefusal(const UObject* Asset)
{
	const FString AssetName = Asset ? Asset->GetName() : TEXT("asset");
	return FString::Printf(
		TEXT("'%s' has an open asset editor. That editor holds its own copy and ")
		TEXT("overwrites this asset when it closes, so writing now would be ")
		TEXT("silently discarded. Close the editor for '%s' and retry."),
		*AssetName, *AssetName);
}
}
#endif

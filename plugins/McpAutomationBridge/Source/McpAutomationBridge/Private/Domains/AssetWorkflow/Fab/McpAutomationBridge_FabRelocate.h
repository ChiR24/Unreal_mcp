// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"
#include "McpFabProvider.h"

/**
 * Moves what one Fab import created out of the machine-named folders Fab chose, and names it.
 *
 * A Megascans surface lands as /Game/Fab/Megascans/3D/concrete_barrier_ubitfhtfa_ue_high_zip_ubitfhtfa/High/
 * ubitfhtfa_tier_1/StaticMeshes/ubitfhtfa_tier_1, with MI_ubitfhtfa and T_ubitfhtfa_4K_* beside it. The
 * caller can name a /Game folder for it and a name for the asset, and this applies both once the import has
 * settled, through the move and rename path asset.move uses: references and settings follow, and the
 * redirectors that are left are fixed.
 *
 * Only what this import itself created under /Game/Fab/ is touched. The master materials, material functions
 * and textures in the shared /Game/Fab/Materials, MaterialFunctions and Textures folders are installed once
 * and used by every later import, as is anything directly in /Game/Fab, so they stay where they are. A pack
 * arrives under /Game/<PackName>, outside that folder, and is left where Fab put it too.
 */
namespace McpFabRelocate
{
/** True for a name an asset can take: a letter or underscore, then letters, digits and underscores, 64 at most. */
bool IsValidAssetName(const FString& Name);

/**
 * DestinationFolder is a validated /Game folder, or empty to leave each asset in its own folder. AssetName,
 * when not empty, becomes the name of the import's one mesh (SM_<AssetName>, or SK_ for a skeletal mesh) and
 * its materials and textures follow it; with no mesh, or several, it is not applied and the note says so.
 * What is under the import's own folder keeps its layout beneath DestinationFolder. ImportedPaths is replaced by
 * the object paths the assets have afterwards; Result gets the new root, the sample paths, how many assets
 * moved, and a note for anything that was not done.
 */
void Apply(const FString& DestinationFolder, const FString& AssetName, FMcpFabAddResult& Result,
	TArray<FString>& ImportedPaths);
} // namespace McpFabRelocate

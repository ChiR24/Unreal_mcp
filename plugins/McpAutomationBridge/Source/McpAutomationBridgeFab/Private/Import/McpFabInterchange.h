// Copyright (c) 2024 MCP Automation Bridge Contributors

#pragma once

#include "CoreMinimal.h"

/**
 * The one lever this adapter has on Fab's mesh import: Interchange's "combine meshes" setting.
 *
 * Fab's generic importer forces every mesh in a source file into ONE static mesh (its own generated
 * Interchange pipeline sets combining to "all", whatever the project settings say). A scene-sized file
 * became a single mesh needing 14 GB to build and held the editor for many minutes.
 *
 * Interchange is reached by reflection only -- class and property names looked up at run time -- so
 * this module takes no build dependency on it and quietly does nothing on an engine that lacks it.
 * Nothing here changes a project setting or an asset: it edits the pipeline instances Fab generated
 * for one import, which Fab discards when that import ends.
 */
namespace McpFabInterchange
{
/** True when this engine's Interchange mesh pipeline has a combine setting that SeparateMeshes can reach. */
bool CanSeparateMeshes();

/**
 * Switches "combine meshes" off on every Interchange pipeline Fab generated and has not yet run,
 * returning how many it changed. Fab's pipelines are the rooted ones; the project's own are assets.
 */
int32 SeparateMeshes();

/**
 * Asks Interchange to cancel every import task it is running, through the manager's own cancel.
 * It stops a translation that is under way; a mesh build already holding the game thread cannot be
 * reached. Returns false when this engine has no Interchange manager to ask.
 */
bool CancelTasks();

/** True while Interchange reports an import in flight, asked through the manager's own IsInterchangeActive. */
bool IsActive();
} // namespace McpFabInterchange

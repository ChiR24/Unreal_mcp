#pragma once

#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsBlueprintGraph.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsResponses.h"

class UStaticMesh;

namespace McpHandlerUtils
{
// NumLODs source models, each LOD halving the previous one's triangle budget; builds and saves.
void ApplyProgressiveLods(UStaticMesh* Mesh, int32 NumLODs);
}

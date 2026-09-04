#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

// get_asset_graph material branch. A material's own graph is its expression
// list, not its package dependencies: the registry BFS in
// McpAutomationBridge_AssetWorkflowDependencies.cpp reports package references
// only, so a material whose expressions touch no /Game assets answered with
// { "<materialPath>": [] } — an empty body exactly where the caller asked
// about the graph. Builds the same envelope the dependency walk returns
// (success, graph, nodeCount, maxDepth, truncated) with graph[materialPath]
// carrying the material's expression nodes and nodeCount reporting the
// array's length. Returns a null pointer when the input is not a UMaterial
// (the caller then keeps the dependency walk) or the object could not be
// resolved; bTruncated is set when the node listing was clamped.
TSharedPtr<FJsonObject> McpTryBuildMaterialGraphResponse(
    const FString& SafeAssetPath,
    int32 MaxDepth,
    bool& bTruncated);

#pragma once

#include "CoreMinimal.h"

class UEdGraphNode;

// Layered (Sugiyama) layout for node graphs: nodes in columns left to right along their links, a node's inputs in the
// column just left of it, crossings reduced by barycentre sweeps, boxes stacked without overlap. Cycles (a loop wired
// back) are laid out as if the back link were not there. Layout is pure geometry; LayoutNodes (McpLayeredNodes.cpp)
// applies it to graph nodes for the Blueprint writer and arrange_nodes, with sizes from McpGraphNodeExtent.h.
namespace McpLayered
{
struct FBox
{
    float Width = 0.0f;
    float Height = 0.0f;
};

struct FEdge
{
    int32 From = INDEX_NONE;
    int32 To = INDEX_NONE;
    // Lower first when a node's links are walked to order a column (exec pins before data, then pin order).
    int32 Order = 0;
    // An exec link; a node with none (a pure value) is placed in the column just left of its first reader.
    bool bExec = true;
};

/** Top-left corner of each box, box 0 kept at Anchor. */
TArray<FVector2D> Layout(const TArray<FBox>& Boxes, const TArray<FEdge>& Edges, const FVector2D& Anchor);

/** Nodes moved by Layout over the links among them (exec links before data, then pin order), Nodes[0] staying where it
 *  is; returns the lowest bottom edge. */
float LayoutNodes(const TArray<UEdGraphNode*>& Nodes);
} // namespace McpLayered

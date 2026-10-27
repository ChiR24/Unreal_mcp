#pragma once

#include "Foundation/GraphLayout/McpGraphNodeExtent.h"

namespace McpGraphLayout
{
// For a node already added at (PosX, PosY): when it overlaps another node,
// removes it from Graph, fills OutMessage/OutDetails (coordinates and free
// slots) and returns true, so the caller replies NODE_OVERLAP. bAutoPlace (a
// call that named no position) first moves it right to the first free slot:
// such a call could only answer the refusal by retrying.
inline bool RefuseOverlappingNode(UEdGraph* Graph, UEdGraphNode* Node, float PosX, float PosY,
	FString& OutMessage, TSharedPtr<FJsonObject>& OutDetails, bool bAutoPlace = false)
{
	float Width = 0.0f;
	float Height = 0.0f;
	EstimateNodeExtent(*Node, Width, Height);
	TArray<FGraphNodeOccupant> Overlapping;
	bool bOverlaps = CheckGraphNodeOverlap(Graph, PosX, PosY, Width, Height, Overlapping, NodeOverlapPadding, Node);
	for (int32 Step = 0; bOverlaps && bAutoPlace && Step < 8; ++Step)
	{
		for (const FGraphNodeOccupant& Occupant : Overlapping)
		{
			PosX = FMath::Max(PosX, Occupant.X + Occupant.Width + NodeSuggestGap);
		}
		Node->NodePosX = PosX;
		bOverlaps = CheckGraphNodeOverlap(Graph, PosX, PosY, Width, Height, Overlapping, NodeOverlapPadding, Node);
	}
	if (!bOverlaps)
	{
		return false;
	}
	Graph->RemoveNode(Node);
	OutDetails = BuildNodeOverlapDetails(PosX, PosY, Width, Height, Overlapping, OutMessage, Graph);
	return true;
}

// A node call that named no position, which RefuseOverlappingNode places instead of refusing.
inline bool NamesNoPosition(const TSharedPtr<FJsonObject>& Payload)
{
	return Payload.IsValid() && !Payload->HasField(TEXT("x")) && !Payload->HasField(TEXT("posX")) &&
		!Payload->HasField(TEXT("y")) && !Payload->HasField(TEXT("posY"));
}
}

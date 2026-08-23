#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Dom/JsonObject.h"

/**
 * Graph-node extent estimation shared by the Blueprint and Material node
 * creation handlers.
 *
 * A node's real on-screen size is decided by Slate when the graph is drawn, so
 * it does not exist on a headless automation path. Callers still have to place
 * nodes by coordinate, and with no size information they cannot avoid stacking
 * them — which is exactly how a material graph ends up with its parameter nodes
 * overlapping. These estimates are derived from pin count and title length and
 * are reported under names that say so, so nobody mistakes one for a
 * measurement.
 */
namespace McpGraphLayout
{
/** Slate metrics a default graph node is laid out with; approximate by design. */
inline constexpr float NodeTitleHeight = 48.0f;
inline constexpr float NodePinRowHeight = 28.0f;
inline constexpr float NodeMinWidth = 224.0f;
inline constexpr float NodeCharWidth = 9.0f;
inline constexpr float NodeTitlePadding = 64.0f;

inline void EstimateNodeExtent(const UEdGraphNode& Node, float& OutWidth, float& OutHeight)
{
	int32 InputRows = 0;
	int32 OutputRows = 0;
	for (const UEdGraphPin* Pin : Node.Pins)
	{
		if (Pin == nullptr || Pin->bHidden)
		{
			continue;
		}
		if (Pin->Direction == EGPD_Input)
		{
			++InputRows;
		}
		else
		{
			++OutputRows;
		}
	}
	const int32 Rows = FMath::Max(InputRows, OutputRows);
	const FString Title = Node.GetNodeTitle(ENodeTitleType::ListView).ToString();
	OutWidth = FMath::Max(NodeMinWidth, Title.Len() * NodeCharWidth + NodeTitlePadding);
	OutHeight = NodeTitleHeight + Rows * NodePinRowHeight;
}

/** Names of nodes whose estimated box intersects NewNode's, excluding itself. */
inline TArray<FString> FindOverlappingNodes(const UEdGraphNode& NewNode)
{
	TArray<FString> Overlapping;
	const UEdGraph* Graph = NewNode.GetGraph();
	if (Graph == nullptr)
	{
		return Overlapping;
	}

	float NewWidth = 0.0f;
	float NewHeight = 0.0f;
	EstimateNodeExtent(NewNode, NewWidth, NewHeight);
	const float NewLeft = static_cast<float>(NewNode.NodePosX);
	const float NewTop = static_cast<float>(NewNode.NodePosY);

	for (const UEdGraphNode* Other : Graph->Nodes)
	{
		if (Other == nullptr || Other == &NewNode)
		{
			continue;
		}
		float OtherWidth = 0.0f;
		float OtherHeight = 0.0f;
		EstimateNodeExtent(*Other, OtherWidth, OtherHeight);
		const float OtherLeft = static_cast<float>(Other->NodePosX);
		const float OtherTop = static_cast<float>(Other->NodePosY);

		const bool bSeparated =
			NewLeft + NewWidth <= OtherLeft || OtherLeft + OtherWidth <= NewLeft ||
			NewTop + NewHeight <= OtherTop || OtherTop + OtherHeight <= NewTop;
		if (!bSeparated)
		{
			Overlapping.Add(Other->GetNodeTitle(ENodeTitleType::ListView).ToString());
		}
	}
	return Overlapping;
}

/**
 * Adds posX/posY plus the estimated extent to a node-creation result, and a
 * human-readable overlap warning when the new node lands on top of others.
 * Returns the warning text so the caller can also surface it on the receipt.
 */
inline FString AddNodePlacementFields(const TSharedPtr<FJsonObject>& Result, const UEdGraphNode& Node)
{
	if (!Result.IsValid())
	{
		return FString();
	}
	float Width = 0.0f;
	float Height = 0.0f;
	EstimateNodeExtent(Node, Width, Height);
	Result->SetNumberField(TEXT("posX"), Node.NodePosX);
	Result->SetNumberField(TEXT("posY"), Node.NodePosY);
	Result->SetNumberField(TEXT("estimatedWidth"), Width);
	Result->SetNumberField(TEXT("estimatedHeight"), Height);

	const TArray<FString> Overlapping = FindOverlappingNodes(Node);
	if (Overlapping.Num() == 0)
	{
		return FString();
	}
	TArray<TSharedPtr<FJsonValue>> OverlapValues;
	for (const FString& Name : Overlapping)
	{
		OverlapValues.Add(MakeShared<FJsonValueString>(Name));
	}
	Result->SetArrayField(TEXT("overlappingNodes"), OverlapValues);

	const FString Warning = FString::Printf(
		TEXT("Node placed at (%d, %d) overlaps %d existing node(s): %s. ")
		TEXT("Estimated size is %.0fx%.0f; offset the next node by at least that ")
		TEXT("height to avoid stacking."),
		Node.NodePosX, Node.NodePosY, Overlapping.Num(),
		*FString::Join(Overlapping, TEXT(", ")), Width, Height);
	Result->SetStringField(TEXT("placementWarning"), Warning);
	return Warning;
}
}
#endif

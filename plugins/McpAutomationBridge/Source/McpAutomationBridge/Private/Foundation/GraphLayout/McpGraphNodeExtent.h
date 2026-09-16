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

/** Distance kept between a new node and its neighbours when refusing a stack. */
inline constexpr float NodeOverlapPadding = 24.0f;
/** Gap left between a refused node and the suggested free slot. */
inline constexpr float NodeSuggestGap = 48.0f;

/** One graph occupant, with everything a caller needs to pick a free slot. */
struct FGraphNodeOccupant
{
	FString Title;
	FString Name;
	int32 X = 0;
	int32 Y = 0;
	float Width = 0.0f;
	float Height = 0.0f;
};

/**
 * Pre-placement overlap test. Call with the new node's intended position and
 * estimated extent BEFORE adding it to the graph: when it returns true the
 * caller must NOT place the node and should report NODE_OVERLAP with the
 * details from BuildNodeOverlapDetails instead.
 *
 * Two boxes count as overlapping when their estimated rectangles intersect
 * after growing the new box by NodeOverlapPadding on every side, so nodes that
 * merely touch edges still pass but anything a reader would call "stacked" is
 * refused.
 */
inline bool CheckGraphNodeOverlap(
	const UEdGraph* Graph, float NewX, float NewY, float NewW, float NewH,
	TArray<FGraphNodeOccupant>& OutOverlapping, float Padding = NodeOverlapPadding,
	const UEdGraphNode* IgnoreNode = nullptr)
{
	OutOverlapping.Reset();
	if (Graph == nullptr)
	{
		return false;
	}
	const float NewLeft = NewX - Padding;
	const float NewTop = NewY - Padding;
	const float NewRight = NewX + NewW + Padding;
	const float NewBottom = NewY + NewH + Padding;

	for (const UEdGraphNode* Other : Graph->Nodes)
	{
		// Callers that check AFTER adding the node to the graph must pass it
		// as IgnoreNode, otherwise every placement trivially "overlaps" itself
		// at its own coordinates.
		if (Other == nullptr || Other == IgnoreNode)
		{
			continue;
		}
		float OtherW = 0.0f;
		float OtherH = 0.0f;
		EstimateNodeExtent(*Other, OtherW, OtherH);
		const float OtherLeft = static_cast<float>(Other->NodePosX);
		const float OtherTop = static_cast<float>(Other->NodePosY);
		const bool bSeparated =
			NewRight <= OtherLeft || OtherLeft + OtherW <= NewX ||
			NewBottom <= OtherTop || OtherTop + OtherH <= NewY;
		if (!bSeparated)
		{
			FGraphNodeOccupant Occupant;
			Occupant.Title = Other->GetNodeTitle(ENodeTitleType::ListView).ToString();
			Occupant.Name = Other->GetName();
			Occupant.X = Other->NodePosX;
			Occupant.Y = Other->NodePosY;
			Occupant.Width = OtherW;
			Occupant.Height = OtherH;
			OutOverlapping.Add(Occupant);
		}
	}
	return OutOverlapping.Num() > 0;
}

/**
 * Packs the refusal payload for NODE_OVERLAP: the requested slot, every
 * occupant (title, object name and full coordinates), and two suggested free
 * slots — one to the right of the pile and one below it — so the caller (or
 * the next AI) can re-run with a concrete position instead of guessing.
 */
inline TSharedPtr<FJsonObject> BuildNodeOverlapDetails(
	float NewX, float NewY, float NewW, float NewH,
	const TArray<FGraphNodeOccupant>& Overlapping,
	FString& OutMessage)
{
	float RightEdge = NewX;
	float BottomEdge = NewY;
	TArray<FString> Names;
	TArray<TSharedPtr<FJsonValue>> OccupantValues;
	for (const FGraphNodeOccupant& Occupant : Overlapping)
	{
		RightEdge = FMath::Max(RightEdge, Occupant.X + Occupant.Width);
		BottomEdge = FMath::Max(BottomEdge, Occupant.Y + Occupant.Height);
		Names.Add(FString::Printf(TEXT("%s @ (%d, %d ~%.0fx%.0f)"),
			*Occupant.Title, Occupant.X, Occupant.Y, Occupant.Width, Occupant.Height));
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("title"), Occupant.Title);
		Entry->SetStringField(TEXT("name"), Occupant.Name);
		Entry->SetNumberField(TEXT("x"), Occupant.X);
		Entry->SetNumberField(TEXT("y"), Occupant.Y);
		Entry->SetNumberField(TEXT("estimatedWidth"), Occupant.Width);
		Entry->SetNumberField(TEXT("estimatedHeight"), Occupant.Height);
		OccupantValues.Add(MakeShared<FJsonValueObject>(Entry));
	}
	const int32 SuggestedRightX = FMath::CeilToInt(RightEdge + NodeSuggestGap);
	const int32 SuggestedBelowY = FMath::CeilToInt(BottomEdge + NodeSuggestGap);

	OutMessage = FString::Printf(
		TEXT("Node placement at (%d, %d ~%.0fx%.0f) overlaps %d existing node(s): %s. ")
		// Deliberately coordinates only, no parameter name: this refusal is
		// shared by callers that spell the position posX/posY and callers that
		// spell it nodePosition, so naming either one sends half of them
		// straight into an UNDECLARED_PARAMETER on the retry.
		TEXT("No node was created. Re-run at a free position — e.g. ")
		TEXT("(%d, %d) (right of the pile) or (%d, %d) (below it); ")
		TEXT("`suggestedPosition` carries the same two points."),
		FMath::CeilToInt(NewX), FMath::CeilToInt(NewY), NewW, NewH,
		Overlapping.Num(), *FString::Join(Names, TEXT(", ")),
		SuggestedRightX, FMath::CeilToInt(NewY), FMath::CeilToInt(NewX), SuggestedBelowY);

	TSharedPtr<FJsonObject> Details = MakeShared<FJsonObject>();
	Details->SetBoolField(TEXT("success"), false);
	Details->SetNumberField(TEXT("requestedX"), NewX);
	Details->SetNumberField(TEXT("requestedY"), NewY);
	Details->SetNumberField(TEXT("requestedWidth"), NewW);
	Details->SetNumberField(TEXT("requestedHeight"), NewH);
	Details->SetArrayField(TEXT("overlappingNodes"), OccupantValues);
	TSharedPtr<FJsonObject> Suggested = MakeShared<FJsonObject>();
	Suggested->SetNumberField(TEXT("x"), SuggestedRightX);
	Suggested->SetNumberField(TEXT("y"), FMath::CeilToInt(NewY));
	Details->SetObjectField(TEXT("suggestedPosition"), Suggested);
	TSharedPtr<FJsonObject> SuggestedBelow = MakeShared<FJsonObject>();
	SuggestedBelow->SetNumberField(TEXT("x"), FMath::CeilToInt(NewX));
	SuggestedBelow->SetNumberField(TEXT("y"), SuggestedBelowY);
	Details->SetObjectField(TEXT("suggestedPositionBelow"), SuggestedBelow);
	return Details;
}
}
#endif

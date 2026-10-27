#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class UWorld;

// manage_ai analyze_level_layout: the level's nav mesh read as a map of spaces (rooms, the corridors and
// alcoves between them, the doors joining them) and how routes run through it: the space or door most routes between the
// others pass (a choke), dead ends, loops, the walk from the start to a goal, what the start cannot reach and how far one
// sees from each space. Reads only. Sampling is McpLevelLayoutSample.cpp, spaces McpLevelLayoutSpaces.cpp.
namespace McpLevelLayout
{
struct FGraph
{
    TArray<FVector> Points;          // nav mesh samples, one or more per grid cell (floors stack)
    TArray<FIntPoint> Cells;         // each sample's grid cell
    TArray<TArray<int32>> Adjacency; // walkable neighbours among the 8 around a sample
    TArray<double> Floor;            // cells of floor each sample stands for: 1 at its cell's centre, 0.5 snapped to a rim
    FIntPoint GridSize = FIntPoint::ZeroValue;
    double CellCm = 100.0;
    FString Note; // the cell had to widen to stay within maxNodes, or the samples ran out
};

struct FSampleSpec
{
    FBox Area = FBox(ForceInit);
    double CellCm = 100.0;
    int32 MaxNodes = 6000;
};

/** World's nav mesh over Spec.Area as a graph of samples; false with OutCode and OutMessage when it cannot be read (no
 *  nav data, a build running, or an area too tall to probe floor by floor). */
bool Sample(UWorld* World, const FSampleSpec& Spec, FGraph& Out, FString& OutCode, FString& OutMessage);

struct FSpace
{
    bool bRoom = false;
    TArray<int32> Nodes;
    int32 Deepest = INDEX_NONE; // the sample farthest from an edge
    int32 Height = 0;           // its distance from an edge, in cells: the space is about 2 * Height + 1 cells wide
    double Floor = 0.0;         // cells of floor (FGraph::Floor of its samples)
    FVector Center = FVector::ZeroVector;
    TArray<int32> Neighbours;
    double PassShare = 0.0; // routes between the other spaces that pass through it
};

struct FDoor
{
    int32 A = 0;
    int32 B = 0;
    FVector Location = FVector::ZeroVector;
    int32 WidthCells = 0;
    double Share = 0.0; // routes between different spaces that cross it
};

struct FLayout
{
    TArray<int32> SpaceOf; // per sample
    TArray<FSpace> Spaces; // rooms first, each kind largest first
    TArray<FDoor> Doors;
    int32 Loops = 0;
};

/** Spaces, doors, loops and route shares of a sampled graph. A sample is in a room when it lies within the inscribed
 *  disk of a sample at least RoomCells / 2 cells from every edge of the graph (the nav mesh's rim, a wall's agent radius
 *  off it); rooms split at narrow doorways (McpWatershed), the rest are corridors. */
FLayout Spaces(const FGraph& Graph, int32 RoomCells);

/** Layout's doors (sample edges between two spaces), each space's neighbours, the loops of the space graph and every
 *  space's and door's share of the routes (McpBetweenness over door distances, spaces weighted by their samples). */
void AddDoorsAndShares(const FGraph& Graph, FLayout& Layout);

/** The reply's rows for Layout (spaces, doors, chokes, dead ends, loops, sightlines traced in World; from a start,
 *  StartNode, with Parent every sample's way back to it: what it cannot reach and the walk to a goal), each list capped;
 *  returns the summary, the reply's message. */
FString WriteReply(UWorld* World, const FGraph& Graph, const FLayout& Layout, const TArray<int32>& Parent, int32 StartNode,
                   int32 GoalNode, const FString& StartName, const FString& GoalName, const TSharedPtr<FJsonObject>& Result);

/** Writes the spaces of Layout as a top-down PNG at Path (doors white, Start green, Goal red); false when it cannot. */
bool WriteImage(const FGraph& Graph, const FLayout& Layout, int32 Start, int32 Goal, const FString& Path);

bool Handle(UEngineWireSubsystem* Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
            TSharedPtr<FMcpBridgeWebSocket> Socket);
} // namespace McpLevelLayout

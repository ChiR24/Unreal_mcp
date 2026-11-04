// Copyright (c) 2024 EngineWire Contributors

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonValue.h"

class AActor;
class FJsonObject;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class UStaticMesh;
class UStaticMeshComponent;
class UWorld;

namespace McpCoplanar
{
// Planes closer than this are one plane to the depth buffer at play distance.
constexpr double McpCoplanarPlaneTolerance = 0.5;

// Two surfaces in one plane facing the same way: the depth test cannot order them, so where they overlap the
// picture flickers between the two (z-fighting). Pillars built flush with the ground block they stand in showed
// it all over a level while every other placement check passed.
struct FMcpCoplanarHit
{
    AActor* Actor = nullptr; // the smaller of the two pieces, the one to nudge
    AActor* OtherActor = nullptr;
    FString Component;
    FString OtherComponent;
    FVector Normal = FVector::ZeroVector; // the way both faces point
    double Gap = 0.0;                     // distance between the two planes
    double OverlapU = 0.0;
    double OverlapV = 0.0;
};

// How much of each side of a mesh's local bounds (-X, +X, -Y, +Y, -Z, +Z) its own outward-facing triangles
// cover: 1 for every side of a cube, about 0.79 for a cylinder's caps, 0 for anything curved.
struct FMcpCoplanarFill
{
    double Share[6] = {};
};
FMcpCoplanarFill MeasureFill(const UStaticMesh* Mesh);

// One side of a mesh's box, in the world.
struct FMcpCoplanarFace
{
    FVector Center = FVector::ZeroVector;
    FVector Normal = FVector::ZeroVector;
    FVector U = FVector::ZeroVector;
    FVector V = FVector::ZeroVector;
    double HalfU = 0.0;
    double HalfV = 0.0;
};

// One visible static mesh component, as the sides its mesh really fills. The pieces of one actor share Owner: moving
// the actor moves them all.
struct FMcpCoplanarPiece
{
    AActor* Actor = nullptr;
    const UStaticMeshComponent* Component = nullptr;
    int32 Owner = 0;
    FBox Bounds = FBox(ForceInit);
    double Volume = 0.0;
    bool bNamed = true;
    FString PinnedWhy;      // why this part cannot move on its own in its actor's Blueprint; empty when it can
    TArray<int32> Carried; // the pieces of its actor attached under it, which move when it moves as a part
    TArray<FMcpCoplanarFace, TInlineAllocator<6>> Faces;
};

// Adds the sides of Box (a mesh's local bounds) under Transform that Fill says the mesh fills.
void AddBoxFaces(const FBox& Box, const FTransform& Transform, const FMcpCoplanarFill& Fill, FMcpCoplanarPiece& Piece);

// Whether B lies in A's plane, faces the same way and overlaps it by more than an edge.
bool FacesInOnePlane(const FMcpCoplanarFace& A, const FMcpCoplanarFace& B, double& OutGap, double& OutU, double& OutV);

// Two faces in one plane: Face of Piece, the smaller piece and the one to nudge, and OtherFace of Other.
struct FMcpCoplanarPair
{
    int32 Piece = 0;
    int32 Face = 0;
    int32 Other = 0;
    int32 OtherFace = 0;
    FVector Normal = FVector::ZeroVector; // the way both faces point
    double Gap = 0.0;
    double OverlapU = 0.0;
    double OverlapV = 0.0;
    double FaceArea = 0.0; // the whole of Piece's face
};

// The visible static mesh components of World. Only faces a mesh really fills count (a cube's sides, a cylinder's
// caps), never the empty sides of a bounding box. With a NameFilter, a piece whose actor's label does not contain it
// is unnamed. With Focus, the focus actors are the named ones and only the pieces near them are measured: a spawn or a
// move costs its neighbourhood, not the level.
TArray<FMcpCoplanarPiece> CollectPieces(UWorld* World, const FString& NameFilter, const TArray<AActor*>* Focus = nullptr);

// Every face pair in one plane among Pieces, largest overlap first, leaving out pairs of two unnamed pieces. With
// bBestOnly, only the largest pair of each two pieces.
TArray<FMcpCoplanarPair> FindPairs(const TArray<FMcpCoplanarPiece>& Pieces, bool bBestOnly);

// FindPairs without the pairs no camera can see.
TArray<FMcpCoplanarPair> FindVisiblePairs(UWorld* World, const TArray<FMcpCoplanarPiece>& Pieces, bool bBestOnly);

// Every coplanar pair of pieces among the visible static mesh components of World, largest overlap first, as
// CollectPieces reads them.
TArray<FMcpCoplanarHit> FindCoplanarFaces(UWorld* World, const FString& NameFilter,
                                          const TArray<AActor*>* Focus = nullptr);

// One actor's coplanar faces, ready for a placement report: its worst pair in words, every pair as data.
struct FMcpCoplanarReport
{
    AActor* Actor = nullptr;
    double Severity = 0.0; // side of the worst overlapping patch, in world units
    FString Issue;
    TArray<TSharedPtr<FJsonValue>> Faces;
};

// FindCoplanarFaces grouped by the actor to nudge, worst first.
TArray<FMcpCoplanarReport> ReportCoplanarFaces(UWorld* World, const FString& NameFilter);

// The pairs that touch the actors a spawn or a move placed, written into each one's placement row (coplanarFaces, and
// the largest pair in placementWarning), one scan of their neighbourhood for the whole call. Only audit_placement
// looked, so a room shell built with its walls up to the ceiling's top flickered while every spawn and move answered
// clean. Returns the rows that had no warning before.
int32 AddCoplanarToRows(const TMap<AActor*, TSharedPtr<FJsonObject>>& Rows);

// "+X" for a face that points along an axis, so it reads like the gizmo arrows.
FString DescribeDirection(const FVector& Normal);

// One move of a fix_coplanar pass: Piece's actor goes Delta, or Piece alone when the pair is two parts of one actor.
struct FMcpCoplanarMove
{
    int32 Pair = 0;
    int32 Piece = 0;
    FVector Delta = FVector::ZeroVector;
};

// One pass planned: the moves in the order made, where each piece ends up, and each pair no move parts, with why.
struct FMcpCoplanarSettle
{
    TArray<FMcpCoplanarMove> Moves;
    TArray<FVector> Offsets;
    TArray<TPair<int32, FString>> Left;
};

// One fix_coplanar pass over Pairs, largest first, each move made on the plan so far: a move is kept only when it
// parts its pair and the piece it moves meets no face it did not meet before.
FMcpCoplanarSettle SettleCoplanar(const TArray<FMcpCoplanarPiece>& Pieces, const TArray<FMcpCoplanarPair>& Pairs,
                                  double Distance);

// control_actor fix_coplanar: moves the actor of every coplanar pair a unit so one surface is clearly in front.
bool HandleFixCoplanar(UEngineWireSubsystem* Bridge, const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket);

// Readies the pairs inside one actor for a pass: only the first placed copy of a Blueprint is planned (one template
// edit fixes every copy), and a part its Blueprint cannot move on its own is pinned, with why. PinnedBlueprints holds
// the Blueprints an earlier pass edited or could not edit.
void PrepareParts(TArray<FMcpCoplanarPiece>& Pieces, TArray<FMcpCoplanarPair>& Pairs,
                  const TMap<FString, FString>& PinnedBlueprints);

// One planned move of a Blueprint part, in the world.
struct FMcpPartMove
{
    AActor* Actor = nullptr;
    const UStaticMeshComponent* Component = nullptr;
    FVector Delta = FVector::ZeroVector;
    FString Off; // the part it moves off
};

// Moves each part in its Blueprint's component template, so every placed copy takes it (propagated, compiled,
// saved), once however many copies are placed; a dry run only lists the moves.
void MovePartsInBlueprints(const TArray<FMcpPartMove>& Moves, bool bDryRun, TMap<FString, FString>& PinnedBlueprints,
                           TArray<TSharedPtr<FJsonValue>>& OutDone, TArray<FString>& OutSkipped);
} // namespace McpCoplanar

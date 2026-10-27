// Copyright (c) 2024 EngineWire Contributors
#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"

class FJsonObject;
class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class UStaticMesh;

// A modular kit laid out by wave function collapse, the part of assemble_modules that never touches a level. Each piece
// fits one cell; each variant (a piece turned by quarter turns about Z) has a profile on each of the cell's six faces, a
// 16 x 16 occupancy read by rays cast in from just outside the face. Two cells may sit side by side only where the faces
// between them match: both air, or occupied squares overlapping by IoU 0.9 or more, judged pair by pair (the likeness is
// not transitive, so no face is filed under a socket id). Sides run +X, -X, +Y, -Y, +Z, -Z; Side ^ 1 is the opposite.
namespace McpModuleKit
{
constexpr int32 ProfileSide = 16;

struct FModuleFace
{
    uint64 Bits[4] = {0, 0, 0, 0};

    void Set(int32 U, int32 V) { Bits[(V * ProfileSide + U) >> 6] |= uint64(1) << ((V * ProfileSide + U) & 63); }
    bool IsAir() const { return (Bits[0] | Bits[1] | Bits[2] | Bits[3]) == 0; }
};

bool Matches(const FModuleFace& A, const FModuleFace& B);
const TCHAR* SideName(int32 Side);

struct FModuleVariant
{
    int32 Module = INDEX_NONE;  // INDEX_NONE: an empty cell
    int32 Turns = 0;            // quarter turns about +Z
    double Weight = 1.0;
    FModuleFace Faces[6];
};

/** Mesh space to the cell's own (centred on the cell, axis-aligned): the piece's cell centre moved to the origin, then
    turned by Turns quarter turns about +Z. Face sampling and the placed instance both go through it. */
FTransform CellFromMesh(const FVector& CentreInMesh, int32 Turns);

/** The six faces of a mesh in a cell of CellSize: rays start 1% of the cell's depth outside each face and look in, and a
    hit within 3% of the depth of where they start marks the square. Tree holds the mesh in mesh space. */
void SampleFaces(const UE::Geometry::FDynamicMeshAABBTree3& Tree, const FVector& CellSize, const FTransform& CellFromMeshT,
                 FModuleFace OutFaces[6]);

struct FModuleProblem
{
    FIntVector Size = FIntVector(1, 1, 1);
    TArray<FModuleVariant> Variants;
    bool OpenSide[6] = {false, false, false, false, false, true};  // open: anything may face out; else only air
    int32 Seed = 1;
    int32 MaxBacktracks = 1000;
    int32 Attempts = 5;
    double Deadline = 0.0;  // FPlatformTime::Seconds() past which it gives up
};

struct FModuleLayout
{
    bool bSolved = false;
    TArray<int32> Placed;  // a variant per cell, X fastest, then Y, then Z
    int32 Backtracks = 0;
    int32 Attempt = 0;
    FString Why;
};

/** Minimum-entropy collapse with propagation, a trail to undo, bounded backtracking, then fresh attempts on the next seeds. */
FModuleLayout Solve(const FModuleProblem& In);
/** The faces of a layout that meet a neighbour's face, or the outside, they do not match. */
int32 OpenFaces(const FModuleProblem& In, const TArray<int32>& Placed);
/** Variant faces that no variant matches and the outside on that side does not take: those variants can never be placed. */
TArray<FIntPoint> UnmatchedFaces(const FModuleProblem& In);  // (variant, side)
} // namespace McpModuleKit

namespace McpModuleAssembly
{
struct FModulePiece
{
    UStaticMesh* Mesh = nullptr;
    double Weight = 1.0;
};

struct FModuleRequest
{
    FVector Cell = FVector::ZeroVector;
    FVector Centre = FVector::ZeroVector;  // the cell's centre in a piece's own space, from the pivot
    TArray<FModulePiece> Pieces;
    McpModuleKit::FModuleProblem In;
    double EmptyWeight = 1.0;
    bool bTurn = true;
};

/** The call's parameters (McpModuleRequest.cpp); false with the reason and its error code at the first unusable one. */
bool ReadRequest(const TSharedPtr<FJsonObject>& Payload, FModuleRequest& Out, FString& OutWhy, FString& OutCode);
/** Each piece's faces at each turn it may take, read before Deadline; false with the reason when a piece does not fit its
    cell box or the reading runs out of time. */
bool BuildVariants(FModuleRequest& Request, double Deadline, FString& OutWhy);

// build_environment assemble_modules (McpEnvAssembleModules.cpp).
bool HandleAssembleModules(UEngineWireSubsystem& Bridge, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                           TSharedPtr<FMcpBridgeWebSocket> Socket);
} // namespace McpModuleAssembly

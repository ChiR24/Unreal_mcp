// Copyright (c) 2024 EngineWire Contributors
//
// A kit piece's faces (McpModuleKit.h): which squares of each side of its cell its geometry covers, read by rays cast in
// from just outside, and whether two faces fit together.

#include "Domains/Environment/Modules/McpModuleKit.h"

namespace McpModuleKit
{
namespace
{
const TCHAR* const ModuleSideNames[6] = {TEXT("+X"), TEXT("-X"), TEXT("+Y"), TEXT("-Y"), TEXT("+Z"), TEXT("-Z")};
constexpr double ModuleQuarterTurn = 1.5707963267948966;
} // namespace

const TCHAR* SideName(int32 Side)
{
    return ModuleSideNames[Side];
}

bool Matches(const FModuleFace& A, const FModuleFace& B)
{
    int32 Both = 0, Either = 0;
    for (int32 Word = 0; Word < 4; ++Word)
    {
        Both += FMath::CountBits(A.Bits[Word] & B.Bits[Word]);
        Either += FMath::CountBits(A.Bits[Word] | B.Bits[Word]);
    }
    // Both air, or IoU of at least 0.9: a corridor 0.3 of the face wide never meets one 0.4 wide.
    return Either == 0 || 10 * Both >= 9 * Either;
}

FTransform CellFromMesh(const FVector& CentreInMesh, int32 Turns)
{
    const FQuat Turn(FVector::UpVector, (Turns & 3) * ModuleQuarterTurn);
    return FTransform(Turn, Turn.RotateVector(-CentreInMesh));
}

void SampleFaces(const UE::Geometry::FDynamicMeshAABBTree3& Tree, const FVector& CellSize, const FTransform& CellFromMeshT,
                 FModuleFace OutFaces[6])
{
    const FVector Half = CellSize / 2.0;
    for (int32 Side = 0; Side < 6; ++Side)
    {
        OutFaces[Side] = FModuleFace();
        // Each pair of faces reads the same two axes, so the faces either side of a shared plane line up square for square.
        const int32 Axis = Side / 2, U = Axis == 0 ? 1 : 0, V = Axis == 2 ? 1 : 2;
        const double Sign = (Side & 1) ? -1.0 : 1.0, Depth = CellSize[Axis];
        FVector Inward = FVector::ZeroVector;
        Inward[Axis] = -Sign;
        const FVector Direction = CellFromMeshT.InverseTransformVectorNoScale(Inward);
        // A square is covered when any of 4 x 4 rays across it hits: one ray at its middle missed a wall 1/40 of the
        // cell thick, which then read as an open face.
        constexpr int32 Sub = 4;
        for (int32 Row = 0; Row < ProfileSide; ++Row)
        {
            for (int32 Column = 0; Column < ProfileSide; ++Column)
            {
                for (int32 Ray = 0; Ray < Sub * Sub; ++Ray)
                {
                    FVector Start = FVector::ZeroVector;
                    Start[Axis] = Sign * (Half[Axis] + 0.01 * Depth);
                    Start[U] = -Half[U] + (Column + (Ray % Sub + 0.5) / Sub) * CellSize[U] / ProfileSide;
                    Start[V] = -Half[V] + (Row + (Ray / Sub + 0.5) / Sub) * CellSize[V] / ProfileSide;
                    double Distance = 0.0;
                    int Triangle = -1;
                    if (Tree.FindNearestHitTriangle(FRay3d(CellFromMeshT.InverseTransformPositionNoScale(Start), Direction), Distance, Triangle) &&
                        Distance <= 0.03 * Depth)
                    {
                        OutFaces[Side].Set(Column, Row);
                        break;
                    }
                }
            }
        }
    }
}
} // namespace McpModuleKit

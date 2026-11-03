// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Environment/Decals/McpDecalPlacement.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Math/RotationMatrix.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpDecalPlacementTest, "EngineWire.Environment.DecalPlacement",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers: a decal's texture runs down its +Y and right along its +Z. On a floor it points down with the texture's
// top toward +X; on a wall facing +Y (seen from +Y, whose right is +X) it points into the wall, upright and unmirrored; a
// turn spins it about the normal, counter-clockwise from outside; a scatter keeps its spacing, its disc and its seed.
bool FMcpDecalPlacementTest::RunTest(const FString& Parameters)
{
    using namespace McpDecalPlacement;
    const FRotationMatrix Floor(SurfaceRotation(FVector::UpVector, 0.0));
    TestTrue(TEXT("floor: +X points down"), Floor.GetUnitAxis(EAxis::X).Equals(FVector(0.0, 0.0, -1.0), 1e-4));
    TestTrue(TEXT("floor: the texture's top toward +X"), Floor.GetUnitAxis(EAxis::Y).Equals(FVector(-1.0, 0.0, 0.0), 1e-4));
    TestTrue(TEXT("floor: the texture's right toward +Y"), Floor.GetUnitAxis(EAxis::Z).Equals(FVector(0.0, 1.0, 0.0), 1e-4));
    const FRotationMatrix Wall(SurfaceRotation(FVector(0.0, 1.0, 0.0), 0.0));
    TestTrue(TEXT("wall: +X points into the wall"), Wall.GetUnitAxis(EAxis::X).Equals(FVector(0.0, -1.0, 0.0), 1e-4));
    TestTrue(TEXT("wall: the texture's down is down"), Wall.GetUnitAxis(EAxis::Y).Equals(FVector(0.0, 0.0, -1.0), 1e-4));
    TestTrue(TEXT("wall: the texture's right is the viewer's right"), Wall.GetUnitAxis(EAxis::Z).Equals(FVector(1.0, 0.0, 0.0), 1e-4));
    const FRotationMatrix Turned(SurfaceRotation(FVector::UpVector, 90.0));
    TestTrue(TEXT("a turn keeps +X on the normal"), Turned.GetUnitAxis(EAxis::X).Equals(FVector(0.0, 0.0, -1.0), 1e-4));
    TestTrue(TEXT("a quarter turn brings the texture's down onto its right"), Turned.GetUnitAxis(EAxis::Y).Equals(FVector(0.0, 1.0, 0.0), 1e-4));

    const FVector Center(100.0, 0.0, 50.0);
    const FVector Down(0.0, 0.0, -1.0);
    const TArray<FVector> Points = ScatterInDisc(Center, Down, 500.0, 20, 3, 80.0);
    TestTrue(TEXT("the same seed gives the same points"), Points == ScatterInDisc(Center, Down, 500.0, 20, 3, 80.0));
    TestTrue(TEXT("another seed gives others"), Points != ScatterInDisc(Center, Down, 500.0, 20, 4, 80.0));
    TestEqual(TEXT("twenty points fit"), Points.Num(), 20);
    for (int32 A = 0; A < Points.Num(); ++A)
    {
        TestTrue(TEXT("inside the disc, at its height"), FVector::Dist2D(Points[A], Center) <= 500.0 + 1e-6 && Points[A].Z == Center.Z);
        for (int32 B = A + 1; B < Points.Num(); ++B)
        {
            TestTrue(TEXT("spaced"), FVector::Dist2D(Points[A], Points[B]) >= 80.0);
        }
    }
    TestTrue(TEXT("a crowded disc gives fewer"), ScatterInDisc(Center, Down, 50.0, 20, 3, 80.0).Num() < 20);
    // Traced into a wall facing +Y, the disc stands upright in the wall's plane: across along X, up along Z.
    const TArray<FVector> OnWall = ScatterInDisc(Center, FVector(0.0, -1.0, 0.0), 500.0, 20, 3, 80.0);
    TestTrue(TEXT("a wall's disc keeps the centre's depth"), OnWall.Num() == 20 && OnWall.FindByPredicate([&](const FVector& P) { return P.Y != Center.Y; }) == nullptr);
    TestTrue(TEXT("and spreads up the wall"), OnWall.FindByPredicate([&](const FVector& P) { return FMath::Abs(P.Z - Center.Z) > 100.0; }) != nullptr);
    return true;
}
#endif

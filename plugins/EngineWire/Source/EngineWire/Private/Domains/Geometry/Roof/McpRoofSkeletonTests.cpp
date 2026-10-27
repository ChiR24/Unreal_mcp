// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Geometry/Roof/McpRoofSkeleton.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpRoofSkeletonTest, "EngineWire.Geometry.RoofSkeleton",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
double RoofTop(const McpRoofSkeleton::FResult& Roof)
{
    double Top = 0.0;
    for (const McpRoofSkeleton::FNode& Node : Roof.Nodes)
    {
        Top = FMath::Max(Top, Node.Time);
    }
    return Top;
}

bool RoofHasNode(const McpRoofSkeleton::FResult& Roof, double X, double Y)
{
    return Roof.Nodes.ContainsByPredicate([X, Y](const McpRoofSkeleton::FNode& Node) { return FVector2D::Distance(Node.Point, FVector2D(X, Y)) < 0.5; });
}

// Every edge of the faces and of the base (the outline) is shared by exactly two of them: the roof is closed.
bool RoofWatertight(const McpRoofSkeleton::FResult& Roof)
{
    TMap<TPair<int32, int32>, int32> Uses;
    const auto Count = [&Uses](int32 A, int32 B) { ++Uses.FindOrAdd(TPair<int32, int32>(FMath::Min(A, B), FMath::Max(A, B))); };
    for (const TArray<int32>& Face : Roof.Faces)
    {
        for (int32 K = 0; K < Face.Num(); ++K)
        {
            Count(Face[K], Face[(K + 1) % Face.Num()]);
        }
    }
    for (int32 I = 0; I < Roof.Outline.Num(); ++I)
    {
        Count(I, (I + 1) % Roof.Outline.Num());
    }
    for (const TPair<TPair<int32, int32>, int32>& Edge : Uses)
    {
        if (Edge.Value != 2)
        {
            return false;
        }
    }
    return Uses.Num() > 0;
}

// The faces cover the outline exactly once: their projected areas add up to its own (the nudge moves it by a hair).
bool RoofCovers(const McpRoofSkeleton::FResult& Roof)
{
    const auto Area = [&Roof](const TArray<FVector2D>& Points)
    {
        double Sum = 0.0;
        for (int32 K = 0; K < Points.Num(); ++K)
        {
            Sum += Points[K].X * Points[(K + 1) % Points.Num()].Y - Points[K].Y * Points[(K + 1) % Points.Num()].X;
        }
        return Sum / 2.0;
    };
    double Faces = 0.0;
    for (const TArray<int32>& Face : Roof.Faces)
    {
        TArray<FVector2D> Points;
        for (const int32 Node : Face)
        {
            Points.Add(Roof.Nodes[Node].Point);
        }
        Faces += Area(Points);
    }
    const double Outline = Area(Roof.Outline);
    return Outline > 0.0 && FMath::Abs(Faces - Outline) < 1e-5 * Outline + 0.05 * Roof.Outline.Num();
}

// Every roof of a family closes, covers its outline and is watertight with its base.
bool RoofSound(const McpRoofSkeleton::FResult& Roof)
{
    return Roof.Error.IsEmpty() && Roof.Faces.Num() == Roof.Outline.Num() && RoofWatertight(Roof) && RoofCovers(Roof);
}
} // namespace

// Known answers: a 10 x 6 m rectangle rises to a 4 m ridge 3 m in; a square to one apex; a 6-8-10 triangle to its
// incenter (2 m in); an L with equal 5 m arms (simultaneous events) to 2.5 m, closed; a footprint drawn clockwise reads
// the same; a bow tie is refused; an outline moved 50 cm outward grows by 50 cm on every side.
bool FMcpRoofSkeletonTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpRoofSkeleton;
    const FResult Rect = Build({FVector2D(0, 0), FVector2D(1000, 0), FVector2D(1000, 600), FVector2D(0, 600)});
    TestTrue(TEXT("rectangle: four faces, closed"), Rect.Error.IsEmpty() && Rect.Faces.Num() == 4 && RoofWatertight(Rect));
    TestEqual(TEXT("rectangle: the ridge is 3 m in"), RoofTop(Rect), 300.0, 0.1);
    TestTrue(TEXT("rectangle: ridge ends"), RoofHasNode(Rect, 300, 300) && RoofHasNode(Rect, 700, 300));

    const FResult Square = Build({FVector2D(0, 0), FVector2D(800, 0), FVector2D(800, 800), FVector2D(0, 800)});
    TestTrue(TEXT("square: one apex"), RoofHasNode(Square, 400, 400) && FMath::IsNearlyEqual(RoofTop(Square), 400.0, 0.1));

    const FResult Triangle = Build({FVector2D(0, 0), FVector2D(600, 0), FVector2D(0, 800)});
    TestTrue(TEXT("triangle: its incenter"), RoofHasNode(Triangle, 200, 200) && FMath::IsNearlyEqual(RoofTop(Triangle), 200.0, 0.1));

    const FResult L = Build({FVector2D(0, 0), FVector2D(1000, 0), FVector2D(1000, 500), FVector2D(500, 500), FVector2D(500, 1000), FVector2D(0, 1000)});
    TestTrue(TEXT("L: six faces, closed"), L.Error.IsEmpty() && L.Faces.Num() == 6 && RoofWatertight(L));
    TestEqual(TEXT("L: 2.5 m in"), RoofTop(L), 250.0, 0.1);

    const FResult Clockwise = Build({FVector2D(0, 0), FVector2D(0, 600), FVector2D(1000, 600), FVector2D(1000, 0)});
    TestEqual(TEXT("clockwise reads the same"), RoofTop(Clockwise), 300.0, 0.1);

    TestFalse(TEXT("a bow tie is refused"), Build({FVector2D(0, 0), FVector2D(100, 100), FVector2D(100, 0), FVector2D(0, 100)}).Error.IsEmpty());

    // Families the brute force must survive: star-like outlines of 5 to 64 corners (many reflex corners), and combs of
    // 2 to 10 equal teeth (many events at once).
    FRandomStream Random(7);
    int32 Unsound = 0;
    for (int32 Trial = 0; Trial < 120; ++Trial)
    {
        const int32 Corners = 5 + Random.RandRange(0, 59);
        TArray<FVector2D> Star;
        for (int32 K = 0; K < Corners; ++K)
        {
            const double Angle = 2.0 * 3.14159265358979323846 * K / Corners, Radius = 300.0 + Random.FRand() * 700.0;
            Star.Add(FVector2D(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle)));
        }
        Unsound += RoofSound(Build(Star)) ? 0 : 1;
    }
    for (int32 Teeth = 2; Teeth <= 10; ++Teeth)
    {
        TArray<FVector2D> Comb = {FVector2D(0, 0), FVector2D(Teeth * 200.0, 0)};
        for (int32 K = Teeth - 1; K >= 0; --K)
        {
            Comb.Append({FVector2D(K * 200.0 + 200.0, 400), FVector2D(K * 200.0 + 100.0, 400), FVector2D(K * 200.0 + 100.0, 100), FVector2D(K * 200.0, 100)});
        }
        Comb.SetNum(Comb.Num() - 2);
        Comb.Add(FVector2D(0, 400));
        Unsound += RoofSound(Build(Comb)) ? 0 : 1;
    }
    TestEqual(TEXT("every star and comb roof closes, covers its outline and is watertight"), Unsound, 0);

    const TArray<FVector2D> Wider = Offset({FVector2D(0, 0), FVector2D(800, 0), FVector2D(800, 800), FVector2D(0, 800)}, 50.0);
    TestTrue(TEXT("offset: 50 cm out on every side"), Wider.Num() == 4 && Wider.ContainsByPredicate([](const FVector2D& P) { return P.Equals(FVector2D(-50, -50), 0.01); }) &&
                                                       Wider.ContainsByPredicate([](const FVector2D& P) { return P.Equals(FVector2D(850, 850), 0.01); }));
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

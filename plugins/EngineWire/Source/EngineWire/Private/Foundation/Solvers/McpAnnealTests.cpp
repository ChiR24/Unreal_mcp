// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Solvers/McpAnneal.h"
#include "Foundation/Solvers/McpBoxSat.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAnnealTest, "EngineWire.Foundation.Solvers.Anneal",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers: the overlap depth of squares side by side and turned, then three boxes stacked on one spot pushed
// apart by annealing their overlap away.
bool FMcpAnnealTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using McpBoxSat::FFootprint;
    const FFootprint Unit{FVector2D(0.0, 0.0), FVector2D(1.0, 1.0), 0.0};
    TestEqual(TEXT("squares 1.5 apart overlap by 0.5"), McpBoxSat::Depth(Unit, {FVector2D(1.5, 0.0), FVector2D(1.0, 1.0), 0.0}), 0.5, 1e-9);
    TestEqual(TEXT("squares 2.5 apart do not overlap"), McpBoxSat::Depth(Unit, {FVector2D(0.0, 2.5), FVector2D(1.0, 1.0), 0.0}), 0.0, 1e-9);
    FVector2D Axis;
    TestEqual(TEXT("and stand 0.5 apart"), McpBoxSat::Separation(Unit, {FVector2D(0.0, 2.5), FVector2D(1.0, 1.0), 0.0}, &Axis), 0.5, 1e-9);
    TestTrue(TEXT("along +Y, from the first toward the second"), Axis.Equals(FVector2D(0.0, 1.0), 1e-9));
    double Distance = 0.0;
    TestTrue(TEXT("a ray from 5 units out meets the square"), McpBoxSat::RayHits(Unit, FVector2D(-5.0, 0.5), FVector2D(1.0, 0.0), Distance));
    TestEqual(TEXT("4 units along it"), Distance, 4.0, 1e-9);
    TestFalse(TEXT("a ray aimed past it misses"), McpBoxSat::RayHits(Unit, FVector2D(-5.0, 1.5), FVector2D(1.0, 0.0), Distance));
    TestFalse(TEXT("a ray aimed away misses"), McpBoxSat::RayHits(Unit, FVector2D(-5.0, 0.0), FVector2D(-1.0, 0.0), Distance));
    // Turned 45 degrees, the second square reaches sqrt(2) along X: 1 + 1.414 - 2.2.
    constexpr double EighthTurn = 0.78539816339744831; // PI is gone in 5.8 and UE_PI missing in 5.0
    const double Turned = McpBoxSat::Depth(Unit, {FVector2D(2.2, 0.0), FVector2D(1.0, 1.0), EighthTurn});
    TestEqual(TEXT("a turned square overlaps by its corner"), Turned, 1.0 + FMath::Sqrt(2.0) - 2.2, 1e-9);

    TArray<FFootprint> Boxes;
    Boxes.Init({FVector2D(0.0, 0.0), FVector2D(50.0, 50.0), 0.0}, 3);
    const auto Overlap = [&Boxes](int32 Index)
    {
        double Sum = 0.0;
        for (int32 Other = 0; Other < Boxes.Num(); ++Other)
        {
            Sum += Other == Index ? 0.0 : FMath::Square(McpBoxSat::Depth(Boxes[Index], Boxes[Other]) / 100.0);
        }
        return Sum;
    };
    double Start = 0.0;
    for (int32 Index = 0; Index < Boxes.Num(); ++Index)
    {
        Start += Overlap(Index) / 2.0;
    }
    int32 Moved = 0;
    FVector2D Was;
    TArray<FFootprint> Best;
    McpAnneal::FSettings Settings;
    Settings.Steps = 4000;
    const McpAnneal::FOutcome Outcome = McpAnneal::Minimize(Start, Settings,
        [&](FRandomStream& Random, double Progress)
        {
            Moved = Random.RandRange(0, Boxes.Num() - 1);
            Was = Boxes[Moved].Center;
            const double Before = Overlap(Moved);
            const double Step = 100.0 * (1.0 - 0.95 * Progress);
            Boxes[Moved].Center += FVector2D(Random.FRandRange(-Step, Step), Random.FRandRange(-Step, Step));
            return Overlap(Moved) - Before;
        },
        [&]() { Boxes[Moved].Center = Was; },
        [&]() { Best = Boxes; });
    Boxes = Best;
    double End = 0.0;
    for (int32 Index = 0; Index < Boxes.Num(); ++Index)
    {
        End += Overlap(Index) / 2.0;
    }
    TestTrue(FString::Printf(TEXT("the pile comes apart (overlap %f m2 from %f)"), End, Start), End < 1e-6 && Start > 0.4);
    TestEqual(TEXT("the best energy is the energy of the best state"), Outcome.BestEnergy, End, 1e-6);
    return true;
}
#endif

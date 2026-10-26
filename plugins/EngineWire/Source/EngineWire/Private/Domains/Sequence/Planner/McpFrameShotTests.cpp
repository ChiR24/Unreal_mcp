// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Sequence/Planner/McpFrameShot.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpFrameShotTest, "EngineWire.Sequence.FrameShot",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers with no world: Aim puts a point exactly where asked; one standing subject lands on the left third at the
// size asked, seen from its front and turned into the frame; two subjects land on both third lines; a wall over every
// place in front of a subject sends the camera round behind it; ground the camera may not sink into keeps it above.
bool FMcpFrameShotTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    McpFrameShot::FLens Lens;
    Lens.TanHalfH = 0.4;
    Lens.TanHalfV = 0.4 / 1.7778;
    FVector2D Screen;
    const FVector Camera(-480.0, 130.0, 260.0);
    const FRotator Aimed = McpFrameShot::Aim(Camera, FVector(20.0, -40.0, 90.0), FVector2D(0.25, -0.4), Lens);
    TestTrue(TEXT("the aimed point is ahead"), McpFrameShot::Project(FVector(20.0, -40.0, 90.0), Camera, Aimed, Lens, Screen));
    TestEqual(TEXT("aimed x"), Screen.X, 0.25, 1e-6);
    TestEqual(TEXT("aimed y"), Screen.Y, -0.4, 1e-6);
    TestEqual(TEXT("no roll"), Aimed.Roll, 0.0, 1e-9);

    McpFrameShot::FProblem One;
    One.Subjects.Add({FVector(0.0, 0.0, 90.0), FVector(30.0, 30.0, 90.0), FVector::ForwardVector});
    One.Targets.Add(FVector2D(-1.0 / 3.0, 0.0));
    One.Lens = Lens;
    One.Fill = 0.55;
    const McpFrameShot::FShot Single = McpFrameShot::Solve(One);
    TestEqual(TEXT("one subject on the left third, x"), Single.Screen[0].X, -1.0 / 3.0, 0.005);
    TestEqual(TEXT("one subject on the left third, y"), Single.Screen[0].Y, 0.0, 0.005);
    TestEqual(TEXT("the size asked"), Single.Fill, 0.55, 0.02);
    TestTrue(TEXT("seen from its front"), Single.Location.X > 0.0);
    TestTrue(TEXT("turned into the frame"), FVector::DotProduct(FVector::ForwardVector, Single.Rotation.RotateVector(FVector::RightVector)) > 0.0);
    TestEqual(TEXT("the same problem, the same shot"), McpFrameShot::Solve(One).Location.X, Single.Location.X, 0.0);

    McpFrameShot::FProblem Two;
    Two.Subjects.Add({FVector(0.0, -150.0, 90.0), FVector(30.0, 30.0, 90.0), FVector::RightVector});
    Two.Subjects.Add({FVector(0.0, 150.0, 90.0), FVector(30.0, 30.0, 90.0), -FVector::RightVector});
    Two.Targets = {FVector2D(-1.0 / 3.0, 0.0), FVector2D(1.0 / 3.0, 0.0)};
    Two.Lens = Lens;
    const McpFrameShot::FShot Pair = McpFrameShot::Solve(Two);
    TestTrue(TEXT("the first on the left third"), FVector2D::Distance(Pair.Screen[0], Two.Targets[0]) < 0.02);
    TestTrue(TEXT("the second on the right third"), FVector2D::Distance(Pair.Screen[1], Two.Targets[1]) < 0.02);

    // Both facing +X, while the order asked (the first on the left) can only be seen from behind them: the camera
    // still keeps them as far away as each other, rather than standing beside the first looking down the line.
    McpFrameShot::FProblem Same = Two;
    Same.Subjects[0].Forward = FVector::ForwardVector;
    Same.Subjects[1].Forward = FVector::ForwardVector;
    const McpFrameShot::FShot Behind = McpFrameShot::Solve(Same);
    const double Ratio = FVector::Dist(Behind.Location, Same.Subjects[0].Center) / FVector::Dist(Behind.Location, Same.Subjects[1].Center);
    TestEqual(TEXT("neither subject dwarfs the other"), Ratio, 1.0, 0.05);
    TestTrue(TEXT("and both stay on their marks"), FVector2D::Distance(Behind.Screen[0], Same.Targets[0]) < 0.02 &&
                                                       FVector2D::Distance(Behind.Screen[1], Same.Targets[1]) < 0.02);

    McpFrameShot::FProblem Walled = One;
    Walled.Occluded = [](const FVector& At, int32) { return At.X > -50.0 ? 1.0 : 0.0; };
    Walled.Inside = [](const FVector& At) { return At.Z < 0.0; };
    const McpFrameShot::FShot Around = McpFrameShot::Solve(Walled);
    TestEqual(TEXT("nothing in the way"), Around.Occluded[0], 0.0, 0.0);
    TestTrue(TEXT("round behind the wall"), Around.Location.X <= -50.0);
    TestFalse(TEXT("above the ground"), Around.bInside);
    TestEqual(TEXT("still on its mark"), Around.Screen[0].X, -1.0 / 3.0, 0.005);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

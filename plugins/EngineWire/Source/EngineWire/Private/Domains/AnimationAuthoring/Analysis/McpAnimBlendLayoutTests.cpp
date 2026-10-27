// Copyright (c) 2024 EngineWire Contributors

#include "Domains/AnimationAuthoring/Analysis/McpAnimBlendLayout.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAnimBlendLayoutTest, "EngineWire.Animation.BlendLayout",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
bool BlendLayoutHas(const McpAnimBlendLayout::FClipPlace& Place, double X, double Y)
{
    return Place.Positions.ContainsByPredicate([X, Y](const FVector& Position) { return Position.Equals(FVector(X, Y, 0.0), 0.05); });
}
} // namespace

// Known answers: a walk (150 forward), a run (450 forward), a walk back (150 back, a hair to the right), a strafe (200
// right) and an idle (3 cm/s). On direction x speed the back walk takes both edges and the idle every direction used, at
// speed 0; on speed alone the back walk's 150 is the walk's and is refused; on right x forward speeds every clip has one
// place and the axes are symmetric.
bool FMcpAnimBlendLayoutTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpAnimBlendLayout;
    const TArray<FVector2D> Clips = {FVector2D(150.0, 0.0), FVector2D(450.0, 0.0), FVector2D(-150.0, 2.0), FVector2D(0.0, 200.0), FVector2D(3.0, 0.0)};
    TArray<FBlendLayoutAxis> Axes;
    TArray<FClipPlace> Places;

    Layout(Clips, {EQuantity::Direction, EQuantity::Speed}, Axes, Places);
    TestTrue(TEXT("direction wraps over -180..180"), Axes[0].bWrap && Axes[0].Min == -180.0 && Axes[0].Max == 180.0);
    TestTrue(TEXT("speed runs 0..450"), Axes[1].Min == 0.0 && Axes[1].Max == 450.0);
    TestTrue(TEXT("walk forward"), BlendLayoutHas(Places[0], 0.0, 150.0) && Places[0].Positions.Num() == 1);
    TestTrue(TEXT("run forward"), BlendLayoutHas(Places[1], 0.0, 450.0));
    TestTrue(TEXT("walk back on both edges"), BlendLayoutHas(Places[2], -180.0, 150.0) && BlendLayoutHas(Places[2], 180.0, 150.0));
    TestTrue(TEXT("strafe right at +90"), BlendLayoutHas(Places[3], 90.0, 200.0));
    TestTrue(TEXT("the idle is a row at speed 0"), Places[4].bIdle && Places[4].Positions.Num() == 4 && BlendLayoutHas(Places[4], 90.0, 0.0) &&
                                                    BlendLayoutHas(Places[4], -180.0, 0.0));

    Layout(Clips, {EQuantity::Speed}, Axes, Places);
    TestTrue(TEXT("1D: the back walk's 150 is taken"), Places[2].Positions.Num() == 0 && Places[2].Refused == 1);
    TestTrue(TEXT("1D: the idle at 0"), BlendLayoutHas(Places[4], 0.0, 0.0));

    Layout(Clips, {EQuantity::RightSpeed, EQuantity::ForwardSpeed}, Axes, Places);
    TestTrue(TEXT("signed axes are symmetric"), Axes[0].Min == -200.0 && Axes[0].Max == 200.0 && Axes[1].Min == -450.0 && Axes[1].Max == 450.0);
    TestTrue(TEXT("the back walk behind"), BlendLayoutHas(Places[2], 2.0, -150.0) && Places[2].Positions.Num() == 1);

    FBlendLayoutAxis Speed;
    Speed.Max = 450.0;
    Cover(Speed, 512.0);
    TestEqual(TEXT("a kept sample widens the axis to the next 50"), Speed.Max, 550.0);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Audio/McpAcoustics.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAcousticsTest, "EngineWire.Audio.Acoustics",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
// A closed box room from the origin to Size (cm), its walls facing in: exact ray hits.
bool AcousticsBoxHit(const FVector& Size, const FVector& From, const FVector& Direction, double MaxDistance, McpAcoustics::FHit& Out)
{
    double Best = TNumericLimits<double>::Max();
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        if (FMath::Abs(Direction[Axis]) < 1e-12)
        {
            continue;
        }
        const double Distance = Direction[Axis] > 0.0 ? (Size[Axis] - From[Axis]) / Direction[Axis] : -From[Axis] / Direction[Axis];
        if (Distance >= 0.0 && Distance < Best)
        {
            Best = Distance;
            Out.Normal = FVector::ZeroVector;
            Out.Normal[Axis] = Direction[Axis] > 0.0 ? -1.0 : 1.0;
        }
    }
    Out.Distance = Best;
    return Best <= MaxDistance;
}
} // namespace

// Known answers: in a closed 6 x 5 x 3 m room of absorption 0.1 the rays read its size, its volume and surface within
// 5%, Eyring 1.09 s, and the traced decay within 10% of it, turned on the floor or not; a 30 m corridor reads as long
// (no Eyring) and still has a traced decay; with no walls in reach every ray is open and there is no decay.
bool FMcpAcousticsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FVector Room(600.0, 500.0, 300.0);
    const McpAcoustics::FRoom Box = McpAcoustics::Estimate(
        [&Room](const FVector& From, const FVector& Direction, double MaxDistance, McpAcoustics::FHit& Out) { return AcousticsBoxHit(Room, From, Direction, MaxDistance, Out); },
        FVector(300.0, 250.0, 150.0), 0.1, 5000.0);
    TestEqual(TEXT("volume"), Box.VolumeM3, 90.0, 4.5);
    TestEqual(TEXT("surface"), Box.SurfaceM2, 126.0, 6.3);
    TestEqual(TEXT("closed"), Box.OpenShare, 0.0, 1e-9);
    TestEqual(TEXT("length"), Box.LengthM, 6.0, 0.06);
    TestEqual(TEXT("width"), Box.WidthM, 5.0, 0.05);
    TestEqual(TEXT("height"), Box.HeightM, 3.0, 0.03);
    TestTrue(TEXT("a box room is well proportioned"), Box.Proportion < McpAcoustics::LongRoom);
    TestEqual(TEXT("Eyring"), Box.EyringSeconds.Get(0.0), 1.09, 0.06);
    TestEqual(TEXT("the traced decay agrees with Eyring in a box"), Box.DecaySeconds.Get(0.0), Box.EyringSeconds.Get(0.0), 0.1 * Box.EyringSeconds.Get(1.0));

    const FQuat Turn(FVector::UpVector, FMath::DegreesToRadians(30.0));
    const McpAcoustics::FRoom Turned = McpAcoustics::Estimate(
        [&Room, &Turn](const FVector& From, const FVector& Direction, double MaxDistance, McpAcoustics::FHit& Out)
        {
            const bool bHit = AcousticsBoxHit(Room, Turn.UnrotateVector(From), Turn.UnrotateVector(Direction), MaxDistance, Out);
            Out.Normal = Turn.RotateVector(Out.Normal);
            return bHit;
        },
        Turn.RotateVector(FVector(300.0, 250.0, 150.0)), 0.1, 5000.0);
    TestEqual(TEXT("a room turned on the floor reads its own length"), Turned.LengthM, 6.0, 0.06);
    TestEqual(TEXT("and width"), Turned.WidthM, 5.0, 0.05);

    const FVector Hall(3000.0, 200.0, 300.0);
    const McpAcoustics::FRoom Corridor = McpAcoustics::Estimate(
        [&Hall](const FVector& From, const FVector& Direction, double MaxDistance, McpAcoustics::FHit& Out) { return AcousticsBoxHit(Hall, From, Direction, MaxDistance, Out); },
        FVector(1500.0, 100.0, 150.0), 0.1, 5000.0);
    TestTrue(TEXT("a corridor is long"), Corridor.Proportion > McpAcoustics::LongRoom);
    TestFalse(TEXT("so Eyring does not hold"), Corridor.EyringSeconds.IsSet());
    TestTrue(TEXT("and it still has a traced decay"), Corridor.DecaySeconds.IsSet());

    const McpAcoustics::FRoom Open = McpAcoustics::Estimate(
        [](const FVector&, const FVector&, double, McpAcoustics::FHit&) { return false; }, FVector::ZeroVector, 0.1, 5000.0);
    TestEqual(TEXT("open sky"), Open.OpenShare, 1.0, 1e-9);
    TestFalse(TEXT("no decay outdoors"), Open.DecaySeconds.IsSet());
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

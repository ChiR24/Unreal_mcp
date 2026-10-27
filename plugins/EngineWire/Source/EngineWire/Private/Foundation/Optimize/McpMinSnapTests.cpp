// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Optimize/McpMinSnap.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpMinSnapTest, "EngineWire.Foundation.Optimize.MinSnap",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers: one leg from rest to rest is the degree-7 smoothstep 35s^4 - 84s^5 + 70s^6 - 20s^7 (a quarter of the
// way in time it has covered 0.0705566 of the way; halfway its speed is 2.1875 times the mean); through three points the
// path meets each one, runs on unbroken in velocity, acceleration and jerk, and a symmetric one has no acceleration
// at its middle point.
bool FMcpMinSnapTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TArray<McpMinSnap::FLeg> One;
    TestTrue(TEXT("one leg solves"), McpMinSnap::Solve({FVector(0.0, 0.0, 0.0), FVector(400.0, -200.0, 100.0)}, {2.0}, One));
    TestEqual(TEXT("a quarter of the time"), McpMinSnap::Evaluate(One[0], 0.5).X, 400.0 * 0.0705566, 1e-3);
    TestEqual(TEXT("halfway"), McpMinSnap::Evaluate(One[0], 1.0).Y, -100.0, 1e-6);
    TestEqual(TEXT("speed halfway"), McpMinSnap::Evaluate(One[0], 1.0, 1).X, 2.1875 * 400.0 / 2.0, 1e-6);
    TestTrue(TEXT("at rest at the start"), McpMinSnap::Evaluate(One[0], 0.0, 1).IsNearlyZero(1e-9));
    TestTrue(TEXT("at rest at the end"), McpMinSnap::Evaluate(One[0], 2.0, 1).IsNearlyZero(1e-6));

    const TArray<FVector> Corner = {FVector(0.0, 0.0, 0.0), FVector(300.0, 0.0, 0.0), FVector(300.0, 500.0, 50.0), FVector(-100.0, 600.0, 0.0)};
    TArray<McpMinSnap::FLeg> Legs;
    TestTrue(TEXT("three legs solve"), McpMinSnap::Solve(Corner, {1.0, 2.5, 1.5}, Legs));
    for (int32 Leg = 0; Leg < Legs.Num(); ++Leg)
    {
        TestTrue(TEXT("it meets each point"), McpMinSnap::Evaluate(Legs[Leg], Legs[Leg].Seconds).Equals(Corner[Leg + 1], 1e-6));
        for (int32 Derivative = 1; Derivative < 4 && Leg + 1 < Legs.Num(); ++Derivative)
        {
            TestTrue(TEXT("unbroken at the joint"), McpMinSnap::Evaluate(Legs[Leg], Legs[Leg].Seconds, Derivative)
                                                          .Equals(McpMinSnap::Evaluate(Legs[Leg + 1], 0.0, Derivative), 1e-4));
        }
    }

    TArray<McpMinSnap::FLeg> Line;
    TestTrue(TEXT("a straight line solves"), McpMinSnap::Solve({FVector(0.0), FVector(100.0, 0.0, 0.0), FVector(200.0, 0.0, 0.0)}, {1.0, 1.0}, Line));
    TestTrue(TEXT("no acceleration at the middle of a symmetric path"), McpMinSnap::Evaluate(Line[0], 1.0, 2).IsNearlyZero(1e-6));
    TArray<McpMinSnap::FLeg> Unused;
    TestFalse(TEXT("a leg of no time is refused"), McpMinSnap::Solve({FVector(0.0), FVector(100.0, 0.0, 0.0)}, {0.0}, Unused));
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

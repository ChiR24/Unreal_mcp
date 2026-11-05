// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Optimize/McpChoiceKnapsack.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpChoiceKnapsackTest, "EngineWire.Foundation.Optimize.ChoiceKnapsack",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers, worked by hand: a light (gain 2.0, cost 5), two distance bands that exclude each other (1.0 for 1,
// 1.5 for 2) and a second light (0.6 for 0.5). Needing 1.5, the first band and the second light (1.6 for 1.5) beat the
// wider band alone (1.5 for 2); needing 3.0, the first light and the first band (3.0 for 6); needing 10, nothing reaches;
// needing nothing, nothing is picked; one group never gives two options.
bool FMcpChoiceKnapsackTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using McpChoiceKnapsack::FOption;
    const TArray<FOption> Options = {{0, 2.0, 5.0}, {1, 1.0, 1.0}, {1, 1.5, 2.0}, {2, 0.6, 0.5}};
    bool bReachable = false;
    TestEqual(TEXT("1.5 ms: the narrow band and the small light"), McpChoiceKnapsack::Pick(Options, 1.5, 0.01, bReachable),
              TArray<int32>({1, 3}));
    TestTrue(TEXT("1.5 ms is reachable"), bReachable);
    TestEqual(TEXT("3 ms: the big light and the narrow band"), McpChoiceKnapsack::Pick(Options, 3.0, 0.01, bReachable),
              TArray<int32>({0, 1}));
    TestTrue(TEXT("10 ms: nothing"), McpChoiceKnapsack::Pick(Options, 10.0, 0.01, bReachable).Num() == 0);
    TestFalse(TEXT("10 ms is out of reach"), bReachable);
    TestTrue(TEXT("no need, no pick"), McpChoiceKnapsack::Pick(Options, 0.0, 0.01, bReachable).Num() == 0 && bReachable);
    // Both bands together would reach 2.5 for 3; one group gives one option, so the light is needed, with the
    // cheaper band (3.0 for 6, where the wider band would make 3.5 for 7).
    TestEqual(TEXT("one option per group"), McpChoiceKnapsack::Pick({{1, 1.0, 1.0}, {1, 1.5, 2.0}, {0, 2.0, 5.0}}, 2.5, 0.01, bReachable),
              TArray<int32>({0, 2}));
    // A need far past 4096 steps is counted in coarser steps and still met.
    TestEqual(TEXT("a large need"), McpChoiceKnapsack::Pick({{0, 60.0, 1.0}, {1, 30.0, 1.0}}, 80.0, 0.01, bReachable),
              TArray<int32>({0, 1}));
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

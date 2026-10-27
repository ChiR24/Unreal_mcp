// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Optimize/McpNelderMead.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpNelderMeadTest, "EngineWire.Foundation.Optimize.NelderMead",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers: a bowl centred at (3, -2) is found from the origin; Rosenbrock's valley, the classic hard case, is
// followed from (-1.2, 1) to its minimum at (1, 1); the same start gives the same point twice.
bool FMcpNelderMeadTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const auto Bowl = [](const TArray<double>& P) { return FMath::Square(P[0] - 3.0) + 2.0 * FMath::Square(P[1] + 2.0); };
    const McpNelderMead::FResult Low = McpNelderMead::Minimize(Bowl, {0.0, 0.0}, {1.0, 1.0}, 1e-12, 2000);
    TestEqual(TEXT("the bowl's x"), Low.Point[0], 3.0, 1e-4);
    TestEqual(TEXT("the bowl's y"), Low.Point[1], -2.0, 1e-4);

    const auto Rosenbrock = [](const TArray<double>& P) { return FMath::Square(1.0 - P[0]) + 100.0 * FMath::Square(P[1] - P[0] * P[0]); };
    const McpNelderMead::FResult Valley = McpNelderMead::Minimize(Rosenbrock, {-1.2, 1.0}, {0.5, 0.5}, 1e-14, 5000);
    TestEqual(TEXT("Rosenbrock's x"), Valley.Point[0], 1.0, 1e-3);
    TestEqual(TEXT("Rosenbrock's y"), Valley.Point[1], 1.0, 1e-3);
    TestTrue(TEXT("within its budget"), Valley.Evaluations <= 5000);

    const McpNelderMead::FResult Again = McpNelderMead::Minimize(Rosenbrock, {-1.2, 1.0}, {0.5, 0.5}, 1e-14, 5000);
    TestEqual(TEXT("the same start, the same point"), Again.Point[0], Valley.Point[0], 0.0);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

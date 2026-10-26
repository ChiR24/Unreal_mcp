// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Solvers/McpCassowary.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
using McpCassowary::ERelation;

McpCassowary::FConstraint CassowaryTest(TArray<TPair<int32, double>> Terms, double Constant, ERelation Relation, double Strength)
{
    McpCassowary::FConstraint C;
    C.Terms = MoveTemp(Terms);
    C.Constant = Constant;
    C.Relation = Relation;
    C.Strength = Strength;
    return C;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpCassowaryTest, "EngineWire.Foundation.Solvers.Cassowary",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers: a required bound beats a weak wish; a stronger wish beats a weaker one; an equality fixes the other
// side of a sum; two required bounds that cannot both hold refuse the second; the same equality twice is accepted; and
// a row of two boxes, the first 300 wide at the left, the second 20 after it up to 1000, makes the second 680 wide.
bool FMcpCassowaryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpCassowary;
    {
        FSolver Solver;
        const int32 X = Solver.AddVariable();
        Solver.AddConstraint(CassowaryTest({{X, 1.0}}, -10.0, ERelation::GreaterOrEqual, Required)); // x >= 10
        Solver.AddConstraint(CassowaryTest({{X, 1.0}}, 0.0, ERelation::Equal, Weak));                  // x == 0, weakly
        TestEqual(TEXT("a required bound beats a weak wish"), Solver.Value(X), 10.0, 1e-6);
    }
    {
        FSolver Solver;
        const int32 X = Solver.AddVariable();
        Solver.AddConstraint(CassowaryTest({{X, 1.0}}, -1.0, ERelation::Equal, Weak));
        Solver.AddConstraint(CassowaryTest({{X, 1.0}}, -5.0, ERelation::Equal, Strong));
        Solver.AddConstraint(CassowaryTest({{X, 1.0}}, -2.0, ERelation::Equal, Medium));
        TestEqual(TEXT("the strong wish wins"), Solver.Value(X), 5.0, 1e-6);
    }
    {
        FSolver Solver;
        const int32 X = Solver.AddVariable();
        const int32 Y = Solver.AddVariable();
        TestTrue(TEXT("x + y == 10"), Solver.AddConstraint(CassowaryTest({{X, 1.0}, {Y, 1.0}}, -10.0, ERelation::Equal, Required)));
        TestTrue(TEXT("x == 3"), Solver.AddConstraint(CassowaryTest({{X, 1.0}}, -3.0, ERelation::Equal, Required)));
        TestEqual(TEXT("y follows"), Solver.Value(Y), 7.0, 1e-6);
        TestTrue(TEXT("the same equality again is accepted"), Solver.AddConstraint(CassowaryTest({{X, 1.0}}, -3.0, ERelation::Equal, Required)));
    }
    {
        FSolver Solver;
        const int32 X = Solver.AddVariable();
        TestTrue(TEXT("x <= 5"), Solver.AddConstraint(CassowaryTest({{X, 1.0}}, -5.0, ERelation::LessOrEqual, Required)));
        TestFalse(TEXT("x >= 10 cannot also hold"), Solver.AddConstraint(CassowaryTest({{X, 1.0}}, -10.0, ERelation::GreaterOrEqual, Required)));
    }
    {
        FSolver Solver; // boxes A (left a, width wa) and B (left b, width wb)
        const int32 A = Solver.AddVariable();
        const int32 WA = Solver.AddVariable();
        const int32 B = Solver.AddVariable();
        const int32 WB = Solver.AddVariable();
        Solver.AddConstraint(CassowaryTest({{A, 1.0}}, 0.0, ERelation::Equal, Required));
        Solver.AddConstraint(CassowaryTest({{WA, 1.0}}, -300.0, ERelation::Equal, Strong));
        Solver.AddConstraint(CassowaryTest({{B, 1.0}, {A, -1.0}, {WA, -1.0}}, -20.0, ERelation::Equal, Required)); // b = a + wa + 20
        Solver.AddConstraint(CassowaryTest({{B, 1.0}, {WB, 1.0}}, -1000.0, ERelation::Equal, Required));          // b + wb = 1000
        Solver.AddConstraint(CassowaryTest({{WB, 1.0}}, -100.0, ERelation::Equal, Weak));                         // a weak size wish
        TestEqual(TEXT("B starts 20 after A"), Solver.Value(B), 320.0, 1e-6);
        TestEqual(TEXT("and runs to 1000"), Solver.Value(WB), 680.0, 1e-6);
    }
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

// Copyright (c) 2024 EngineWire Contributors

#include "Foundation/Solvers/McpBetweenness.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpBetweennessTest, "EngineWire.Foundation.Solvers.Betweenness",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers: in a chain a - b - c every route between a and c passes b (share 1) whatever their sizes; in a
// triangle no route passes a third node (share 0); around a square, b carries half of the a - c routes, a sixth of the
// routes between the other three; an edge of the chain carries the routes from a to b and c, four of six; a node cut
// off from the chain lowers none of its shares (no route reaches it).
bool FMcpBetweennessTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const McpBetweenness::FResult Chain = McpBetweenness::Compute(3, {{0, 1, 1.0}, {1, 2, 1.0}}, {1.0, 1.0, 1.0});
    TestEqual(TEXT("the middle of a chain carries every route between its ends"), Chain.NodeShare[1], 1.0, 1e-9);
    TestEqual(TEXT("an end carries none"), Chain.NodeShare[0], 0.0, 1e-9);
    TestEqual(TEXT("an edge of the chain carries four routes of six"), Chain.EdgeShare[0], 4.0 / 6.0, 1e-9);

    const McpBetweenness::FResult Weighted = McpBetweenness::Compute(3, {{0, 1, 5.0}, {1, 2, 2.0}}, {10.0, 1.0, 10.0});
    TestEqual(TEXT("sizes and lengths do not change a chain's middle"), Weighted.NodeShare[1], 1.0, 1e-9);

    const McpBetweenness::FResult Triangle =
        McpBetweenness::Compute(3, {{0, 1, 1.0}, {1, 2, 1.0}, {2, 0, 1.0}}, {1.0, 1.0, 1.0});
    TestEqual(TEXT("no route in a triangle passes a third node"), Triangle.NodeShare[1], 0.0, 1e-9);

    const McpBetweenness::FResult Square =
        McpBetweenness::Compute(4, {{0, 1, 1.0}, {1, 2, 1.0}, {2, 3, 1.0}, {3, 0, 1.0}}, {1.0, 1.0, 1.0, 1.0});
    TestEqual(TEXT("around a square, b carries half the a - c routes"), Square.NodeShare[1], 1.0 / 6.0, 1e-9);

    const McpBetweenness::FResult Island = McpBetweenness::Compute(4, {{0, 1, 1.0}, {1, 2, 1.0}}, {1.0, 1.0, 1.0, 5.0});
    TestEqual(TEXT("a node no route reaches leaves the middle of a chain at 1"), Island.NodeShare[1], 1.0, 1e-9);
    TestEqual(TEXT("and an edge of it at four routes of six"), Island.EdgeShare[0], 4.0 / 6.0, 1e-9);
    TestEqual(TEXT("the cut-off node carries none"), Island.NodeShare[3], 0.0, 1e-9);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

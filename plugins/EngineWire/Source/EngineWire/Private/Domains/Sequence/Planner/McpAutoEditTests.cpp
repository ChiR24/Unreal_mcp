// Copyright (c) 2024 EngineWire Contributors

#include "Domains/Sequence/Planner/McpAutoEdit.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpAutoEditTest, "EngineWire.Sequence.AutoEdit",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
TArray<TArray<double>> EditTestShots(int32 Segments, TFunctionRef<TArray<double>(int32 Segment)> Costs)
{
    TArray<TArray<double>> Out;
    for (int32 Segment = 0; Segment < Segments; ++Segment)
    {
        Out.Add(Costs(Segment));
    }
    return Out;
}

FString EditTestText(const TArray<int32>& Edit)
{
    FString Out;
    for (const int32 Camera : Edit)
    {
        Out.AppendChar(TEXT('A') + Camera);
    }
    return Out;
}
} // namespace

// Known answers for the edit: one cut where the better camera changes; no cut for a preference shorter than the
// shortest shot; shots held no longer than the longest; a camera across the line kept out when cutting to it costs
// more than the others' worse framing.
bool FMcpAutoEditTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const auto Flat = [](int32, int32, int32) { return 0.3; };
    TestEqual(TEXT("one cut at the change"),
              EditTestText(McpAutoEdit::Cut(EditTestShots(12, [](int32 S) { return S < 6 ? TArray<double>{0.0, 1.0} : TArray<double>{1.0, 0.0}; }), Flat, 3, 20)),
              FString(TEXT("AAAAAABBBBBB")));
    TestEqual(TEXT("no cut for two segments"),
              EditTestText(McpAutoEdit::Cut(EditTestShots(12, [](int32 S) { return S == 5 || S == 6 ? TArray<double>{0.4, 0.0} : TArray<double>{0.0, 1.0}; }), Flat, 3, 20)),
              FString(TEXT("AAAAAAAAAAAA")));
    TestEqual(TEXT("shots of four segments at most"),
              EditTestText(McpAutoEdit::Cut(EditTestShots(12, [](int32) { return TArray<double>{0.0, 0.2}; }), [](int32, int32, int32) { return 0.1; }, 3, 4)),
              FString(TEXT("AAAABBBBAAAA")));
    const auto Crossing = [](int32, int32 From, int32 To) { return From == 2 || To == 2 ? 5.0 : 0.3; };
    TestEqual(TEXT("the camera across the line stays out"),
              EditTestText(McpAutoEdit::Cut(EditTestShots(12, [](int32 S) { return S < 6 ? TArray<double>{0.0, 0.5, 2.0} : TArray<double>{0.5, 0.0, 0.0}; }), Crossing, 2, 20)),
              FString(TEXT("AAAAAABBBBBB")));
    TestEqual(TEXT("nothing to cut"), McpAutoEdit::Cut({}, Flat, 2, 4).Num(), 0);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

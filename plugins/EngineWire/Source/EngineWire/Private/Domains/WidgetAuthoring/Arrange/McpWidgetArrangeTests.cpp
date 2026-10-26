#include "Domains/WidgetAuthoring/Arrange/McpWidgetArrange.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
TArray<TSharedPtr<FJsonValue>> ArrangeTestJson(const FString& Text)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Values);
    return Values;
}

McpWidgetArrange::FArrangeBox ArrangeTestBox(double Left, double Top, double Width, double Height)
{
    McpWidgetArrange::FArrangeBox Box;
    Box.Left = Left;
    Box.Top = Top;
    Box.Width = Width;
    Box.Height = Height;
    return Box;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpWidgetArrangeTest, "EngineWire.WidgetAuthoring.Arrange",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers on a 1920 x 1080 panel: a title centred 80 from the top, a 300 wide Play button 40 under it on its
// centre line, and Quit as wide as Play, 20 under it and on its left edge, all three piled at the top-left corner to
// begin with; the title keeps its 400 x 60. Anchors: the title holds the top centre, the buttons follow it. Two widths
// required for one button cannot both hold: the second is named.
bool FMcpWidgetArrangeTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace McpWidgetArrange;
    TArray<FString> Names;
    TArray<FRelation> Relations;
    FString Error;
    const bool bRead = ParseRelations(ArrangeTestJson(TEXT("[{\"type\":\"center\",\"widget\":\"Title\",\"axis\":\"x\"},")
        TEXT("{\"type\":\"inset\",\"widget\":\"Title\",\"edge\":\"top\",\"value\":80},")
        TEXT("{\"type\":\"below\",\"widget\":\"Play\",\"of\":\"Title\",\"gap\":40},{\"type\":\"alignCenterX\",\"widgets\":[\"Play\",\"Title\"]},")
        TEXT("{\"type\":\"width\",\"widget\":\"Play\",\"value\":300},{\"type\":\"below\",\"widget\":\"Quit\",\"of\":\"Play\",\"gap\":20},")
        TEXT("{\"type\":\"sameWidth\",\"widgets\":[\"Quit\",\"Play\"]},{\"type\":\"alignLeft\",\"widgets\":[\"Quit\",\"Play\"]}]")),
        Names, Relations, Error);
    TestTrue(FString::Printf(TEXT("the relations read (%s)"), *Error), bRead && Names.Num() == 3 && Relations.Num() == 8);
    if (!bRead || Names.Num() != 3)
    {
        return false;
    }
    const FVector2D Panel(1920.0, 1080.0);
    const TArray<FArrangeBox> Current = {ArrangeTestBox(0, 0, 400, 60), ArrangeTestBox(0, 0, 200, 50), ArrangeTestBox(0, 0, 200, 50)};
    TArray<FArrangeBox> Boxes;
    int32 Failed = INDEX_NONE;
    TestTrue(TEXT("the layout solves"), Solve(Relations, Current, {FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector}, Panel, Boxes, Failed));
    TestEqual(TEXT("the title is centred"), Boxes[0].Left, 760.0, 1e-3);
    TestEqual(TEXT("80 from the top"), Boxes[0].Top, 80.0, 1e-3);
    TestEqual(TEXT("keeping its size"), Boxes[0].Width, 400.0, 1e-3);
    TestEqual(TEXT("its height too (it does not shrink to pull the buttons up)"), Boxes[0].Height, 60.0, 1e-3);
    TestEqual(TEXT("Play is 300 wide"), Boxes[1].Width, 300.0, 1e-3);
    TestEqual(TEXT("on the title's centre line"), Boxes[1].Left, 810.0, 1e-3);
    TestEqual(TEXT("40 under it"), Boxes[1].Top, 180.0, 1e-3);
    TestEqual(TEXT("Quit is as wide"), Boxes[2].Width, 300.0, 1e-3);
    TestEqual(TEXT("on Play's left edge"), Boxes[2].Left, 810.0, 1e-3);
    TestEqual(TEXT("20 under it"), Boxes[2].Top, 250.0, 1e-3);
    for (const FRelation& Relation : Relations)
    {
        TestEqual(FString::Printf(TEXT("%s holds"), *Relation.Text), Residual(Relation, Boxes, Panel), 0.0, 1e-3);
    }
    const TArray<TPair<EAnchor, EAnchor>> Anchors = InferAnchors(Relations, Boxes, Panel);
    TestTrue(TEXT("the title holds the top centre"), Anchors[0].Key == EAnchor::Middle && Anchors[0].Value == EAnchor::Start);
    TestTrue(TEXT("the buttons follow it"), Anchors[1] == Anchors[0] && Anchors[2] == Anchors[0]);

    TArray<FString> LabelNames;
    TArray<FRelation> Corner;
    ParseRelations(ArrangeTestJson(TEXT("[{\"type\":\"inset\",\"widget\":\"Score\",\"edge\":\"right\",\"value\":24}]")), LabelNames, Corner, Error);
    Solve(Corner, {ArrangeTestBox(40, 24, 160, 40)}, {FVector2D::ZeroVector}, Panel, Boxes, Failed);
    TestEqual(TEXT("a label set in from the right moves there"), Boxes[0].Left, 1736.0, 1e-3);
    TestEqual(TEXT("keeping its width"), Boxes[0].Width, 160.0, 1e-3);

    TArray<FString> TwoNames;
    TArray<FRelation> Twice;
    ParseRelations(ArrangeTestJson(TEXT("[{\"type\":\"width\",\"widget\":\"Play\",\"value\":300},{\"type\":\"width\",\"widget\":\"Play\",\"value\":200}]")),
                   TwoNames, Twice, Error);
    TestFalse(TEXT("two required widths cannot both hold"), Solve(Twice, {ArrangeTestBox(0, 0, 100, 40)}, {FVector2D::ZeroVector}, Panel, Boxes, Failed));
    TestEqual(TEXT("the second is the one named"), Failed, 1);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

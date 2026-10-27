#include "Domains/WidgetAuthoring/Audit/McpWidgetAudit.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

namespace
{
McpWidgetAudit::FItem AuditTestItem(const TCHAR* Name, McpWidgetAudit::EKind Kind, double X0, double Y0, double X1, double Y1)
{
    McpWidgetAudit::FItem Item;
    Item.Name = Name;
    Item.Kind = Kind;
    Item.Rect = FBox2D(FVector2D(X0, Y0), FVector2D(X1, Y1));
    Item.Local = Item.Rect.GetSize();
    Item.Desired = Item.Local;
    Item.Text = Kind == McpWidgetAudit::EKind::Text ? TEXT("Label") : TEXT("");
    return Item;
}

int32 AuditTestCount(const TArray<McpWidgetAudit::FFinding>& Findings, const TCHAR* Kind, const TCHAR* Widget)
{
    return Findings.FilterByPredicate([&](const McpWidgetAudit::FFinding& Finding)
                                      { return Finding.Kind == Kind && (Finding.Widget == Widget || Finding.Other == Widget); }).Num();
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMcpWidgetAuditTest, "EngineWire.WidgetAuthoring.Audit",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Known answers on a 1000 x 500 screen laid out by hand: a label too narrow for its text, two buttons one on the other,
// an icon past the right edge, two stacked labels whose left edges miss by 2 px, a 30 px button with touch on, and a
// label inside a button (which overlaps nothing); WCAG's own ratios for black on white and #777 on white.
bool FMcpWidgetAuditTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using McpWidgetAudit::EKind;
    TArray<McpWidgetAudit::FItem> Items;
    Items.Add(AuditTestItem(TEXT("Narrow"), EKind::Text, 100, 60, 200, 90));
    Items[0].Desired = FVector2D(150.0, 30.0);
    Items.Add(AuditTestItem(TEXT("Play"), EKind::Interactive, 500, 100, 600, 150));
    Items.Add(AuditTestItem(TEXT("Quit"), EKind::Interactive, 550, 120, 650, 170));
    Items.Add(AuditTestItem(TEXT("Icon"), EKind::Image, 950, 400, 1050, 440));
    Items.Add(AuditTestItem(TEXT("Score"), EKind::Text, 120, 300, 220, 330));
    Items.Add(AuditTestItem(TEXT("Time"), EKind::Text, 122, 340, 222, 370));
    Items.Add(AuditTestItem(TEXT("Tiny"), EKind::Interactive, 300, 200, 330, 230));
    Items.Add(AuditTestItem(TEXT("PlayText"), EKind::Text, 505, 105, 545, 145));
    Items.Last().Ancestors = {1};
    Items.Add(AuditTestItem(TEXT("Centred"), EKind::Text, 700, 300, 800, 330)); // centred labels of two widths:
    Items.Add(AuditTestItem(TEXT("Centred2"), EKind::Text, 703, 340, 797, 370)); // left edges 3 apart, centres equal

    McpWidgetAudit::FOptions Options;
    Options.MinTarget = 44.0;
    TArray<McpWidgetAudit::FFinding> Findings;
    McpWidgetAudit::CheckLayout(Items, FIntPoint(1000, 500), Options, Findings);
    TestEqual(TEXT("the narrow label's text is cut off"), AuditTestCount(Findings, TEXT("clipped_text"), TEXT("Narrow")), 1);
    TestEqual(TEXT("the two buttons overlap"), AuditTestCount(Findings, TEXT("overlap"), TEXT("Quit")), 1);
    TestEqual(TEXT("a label inside its button overlaps nothing"), AuditTestCount(Findings, TEXT("overlap"), TEXT("PlayText")), 0);
    TestEqual(TEXT("the icon leaves the screen"), AuditTestCount(Findings, TEXT("off_screen"), TEXT("Icon")), 1);
    TestEqual(TEXT("the stacked labels are 2 px out of line"), AuditTestCount(Findings, TEXT("misaligned"), TEXT("Time")), 1);
    TestEqual(TEXT("two centred labels of two widths are not"), AuditTestCount(Findings, TEXT("misaligned"), TEXT("Centred2")), 0);
    TestEqual(TEXT("the 30 px button is too small to touch"), AuditTestCount(Findings, TEXT("small_target"), TEXT("Tiny")), 1);
    TestEqual(TEXT("the large buttons are not"), AuditTestCount(Findings, TEXT("small_target"), TEXT("Play")), 0);

    TestEqual(TEXT("black on white is 21:1"), McpWidgetAudit::ContrastRatio(FColor::Black, FColor::White), 21.0, 1e-6);
    TestEqual(TEXT("#777 on white is 4.48:1"), McpWidgetAudit::ContrastRatio(FColor(0x77, 0x77, 0x77), FColor::White), 4.48, 0.01);
    return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

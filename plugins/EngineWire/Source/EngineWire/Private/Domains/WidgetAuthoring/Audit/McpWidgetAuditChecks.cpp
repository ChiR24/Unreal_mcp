// Layout findings of audit_widget (see McpWidgetAudit.h): off the screen, text cut off by its box, widgets on top of
// each other, outside the safe area, too small to touch, and edges that almost line up.
#include "Domains/WidgetAuthoring/Audit/McpWidgetAudit.h"

namespace McpWidgetAudit
{
namespace
{
constexpr double AuditOverlapShare = 0.1; // of the smaller widget's area: less is two boxes touching, not one on another
constexpr int32 AuditMaxMisaligned = 20;  // near misses are many on a busy screen: the closest pairs are kept

bool AuditLeaf(const FItem& Item)
{
    return (Item.Kind == EKind::Text || Item.Kind == EKind::Image || Item.Kind == EKind::Interactive) && Item.Rect.bIsValid &&
           Item.Rect.GetArea() > 1.0;
}

FFinding AuditFinding(const TCHAR* Kind, int32 Severity, const FItem& Item, const FString& Detail, const FItem* Other = nullptr)
{
    FFinding Out;
    Out.Kind = Kind;
    Out.Severity = Severity;
    Out.Widget = Item.Name;
    Out.Rect = Item.Rect;
    Out.Detail = Detail;
    if (Other)
    {
        Out.Other = Other->Name;
        Out.Rect += Other->Rect;
    }
    return Out;
}

// How far Inner pokes out of Outer on its worst side, in pixels (0 inside); OutSide names that side.
double AuditOutside(const FBox2D& Inner, const FBox2D& Outer, const TCHAR*& OutSide)
{
    const double Sides[4] = {Outer.Min.X - Inner.Min.X, Inner.Max.X - Outer.Max.X, Outer.Min.Y - Inner.Min.Y, Inner.Max.Y - Outer.Max.Y};
    static const TCHAR* const Names[4] = {TEXT("left"), TEXT("right"), TEXT("top"), TEXT("bottom")};
    int32 Worst = 0;
    for (int32 Side = 1; Side < 4; ++Side)
    {
        Worst = Sides[Side] > Sides[Worst] ? Side : Worst;
    }
    OutSide = Names[Worst];
    return FMath::Max(0.0, Sides[Worst]);
}

bool AuditNested(const TArray<FItem>& Items, int32 A, int32 B)
{
    return Items[A].Ancestors.Contains(B) || Items[B].Ancestors.Contains(A);
}

// Two widgets on top of each other: buttons on buttons, text on text, text across a button it is not part of. An
// image is not judged: images are backdrops and frames by design.
void AuditOverlaps(const TArray<FItem>& Items, TArray<FFinding>& Out)
{
    TArray<int32> Order;
    for (int32 Index = 0; Index < Items.Num(); ++Index)
    {
        if (AuditLeaf(Items[Index]) && !Items[Index].bScrolled && Items[Index].Kind != EKind::Image)
        {
            Order.Add(Index);
        }
    }
    Order.Sort([&Items](int32 L, int32 R) { return Items[L].Rect.Min.X < Items[R].Rect.Min.X; });
    for (int32 I = 0; I < Order.Num(); ++I)
    {
        const FItem& A = Items[Order[I]];
        for (int32 J = I + 1; J < Order.Num() && Items[Order[J]].Rect.Min.X < A.Rect.Max.X; ++J) // sweep along x
        {
            const FItem& B = Items[Order[J]];
            if (!A.Rect.Intersect(B.Rect) || AuditNested(Items, Order[I], Order[J]))
            {
                continue;
            }
            const FBox2D Both(FVector2D::Max(A.Rect.Min, B.Rect.Min), FVector2D::Min(A.Rect.Max, B.Rect.Max));
            const double Share = Both.GetArea() / FMath::Max(1.0, FMath::Min(A.Rect.GetArea(), B.Rect.GetArea()));
            if (Share >= AuditOverlapShare)
            {
                Out.Add(AuditFinding(TEXT("overlap"), 2, A, FString::Printf(TEXT("covers %.0f%% of %s"), Share * 100.0, *B.Name), &B));
            }
        }
    }
}

// Edges of two neighbouring widgets that miss each other by a pixel or few: two labels stacked one under the other whose
// left edges are 2 px apart, two icons side by side whose tops differ by 3.
void AuditMisaligned(const TArray<FItem>& Items, double NearMissPx, TArray<FFinding>& Out)
{
    struct FMiss
    {
        int32 A = 0;
        int32 B = 0;
        double By = 0.0;
        const TCHAR* Line = TEXT("");
    };
    TArray<FMiss> Misses;
    for (int32 A = 0; A < Items.Num(); ++A)
    {
        for (int32 B = A + 1; B < Items.Num(); ++B)
        {
            const FBox2D& RA = Items[A].Rect;
            const FBox2D& RB = Items[B].Rect;
            if (!AuditLeaf(Items[A]) || !AuditLeaf(Items[B]) || Items[A].bScrolled || Items[B].bScrolled || AuditNested(Items, A, B))
            {
                continue;
            }
            // Stacked (vertical lines matter) or side by side (horizontal ones), within twice the larger size.
            const double GapY = FMath::Max(RA.Min.Y - RB.Max.Y, RB.Min.Y - RA.Max.Y);
            const double GapX = FMath::Max(RA.Min.X - RB.Max.X, RB.Min.X - RA.Max.X);
            const bool bStacked = GapY <= 2.0 * FMath::Max(RA.GetSize().Y, RB.GetSize().Y) && GapX < 0.0;
            const bool bBeside = GapX <= 2.0 * FMath::Max(RA.GetSize().X, RB.GetSize().X) && GapY < 0.0;
            const double Lines[6] = {RA.Min.X - RB.Min.X, RA.Max.X - RB.Max.X, RA.GetCenter().X - RB.GetCenter().X,
                                     RA.Min.Y - RB.Min.Y, RA.Max.Y - RB.Max.Y, RA.GetCenter().Y - RB.GetCenter().Y};
            static const TCHAR* const Names[6] = {TEXT("left"), TEXT("right"), TEXT("centre"), TEXT("top"), TEXT("bottom"), TEXT("middle")};
            // Lined up exactly on one line of an axis (two centred labels of different widths), they are not out of line
            // on its others.
            const bool bAlignedX = FMath::Min3(FMath::Abs(Lines[0]), FMath::Abs(Lines[1]), FMath::Abs(Lines[2])) <= 0.5;
            const bool bAlignedY = FMath::Min3(FMath::Abs(Lines[3]), FMath::Abs(Lines[4]), FMath::Abs(Lines[5])) <= 0.5;
            for (int32 Line = 0; Line < 6; ++Line)
            {
                const double By = FMath::Abs(Lines[Line]);
                if ((Line < 3 ? bStacked && !bAlignedX : bBeside && !bAlignedY) && By > 0.5 && By <= NearMissPx)
                {
                    Misses.Add({A, B, By, Names[Line]});
                    break;
                }
            }
        }
    }
    Misses.Sort([](const FMiss& L, const FMiss& R) { return L.By < R.By; });
    for (int32 Index = 0; Index < Misses.Num() && Index < AuditMaxMisaligned; ++Index)
    {
        const FMiss& Miss = Misses[Index];
        Out.Add(AuditFinding(TEXT("misaligned"), 0, Items[Miss.A], FString::Printf(TEXT("its %s edge is %.0f px off %s's"),
                             Miss.Line, Miss.By, *Items[Miss.B].Name), &Items[Miss.B]));
    }
}
} // namespace

void CheckLayout(const TArray<FItem>& Items, FIntPoint Size, const FOptions& Options, TArray<FFinding>& Out)
{
    const FBox2D Screen(FVector2D::ZeroVector, FVector2D(Size));
    const FVector2D Margin = FVector2D(Size) * (1.0 - Options.SafeZone) * 0.5;
    const FBox2D Safe(Margin, FVector2D(Size) - Margin);
    for (const FItem& Item : Items)
    {
        if (!AuditLeaf(Item))
        {
            continue;
        }
        const TCHAR* Side = TEXT("");
        const double Off = Item.bScrolled ? 0.0 : AuditOutside(Item.Rect, Screen, Side);
        if (Off > 0.5)
        {
            Out.Add(AuditFinding(TEXT("off_screen"), 3, Item, Item.Rect.Intersect(Screen)
                ? FString::Printf(TEXT("%.0f px past the %s edge of the screen"), Off, Side) : FString(TEXT("entirely off the screen"))));
        }
        // Over a unit: a text measures a unit wider or narrower at another DPI scale (font hinting), under a pixel.
        const bool bWide = Item.Kind == EKind::Text && !Item.bWraps && Item.Desired.X > Item.Local.X + 1.0;
        const bool bTall = Item.Kind == EKind::Text && Item.Desired.Y > Item.Local.Y + 1.0;
        if (bWide || bTall)
        {
            Out.Add(AuditFinding(TEXT("clipped_text"), 3, Item, FString::Printf(TEXT("'%s' needs %.0f x %.0f, its box is %.0f x %.0f (Slate units)"),
                *Item.Text.Left(40), Item.Desired.X, Item.Desired.Y, Item.Local.X, Item.Local.Y)));
        }
        const double Unsafe = Item.bScrolled || Item.Kind == EKind::Image ? 0.0 : AuditOutside(Item.Rect, Safe, Side);
        if (Options.SafeZone < 1.0 && Unsafe > 0.5 && Off <= 0.5)
        {
            Out.Add(AuditFinding(TEXT("unsafe"), 1, Item, FString::Printf(TEXT("%.0f px into the %s margin outside the title-safe area (safeZone %.2f)"),
                Unsafe, Side, Options.SafeZone)));
        }
        const double Short = FMath::Min(Item.Local.X, Item.Local.Y);
        if (Options.MinTarget > 0.0 && Item.Kind == EKind::Interactive && Short < Options.MinTarget)
        {
            Out.Add(AuditFinding(TEXT("small_target"), 1, Item, FString::Printf(TEXT("%.0f x %.0f, under %.0f on its short side (Slate units)"),
                Item.Local.X, Item.Local.Y, Options.MinTarget)));
        }
    }
    AuditOverlaps(Items, Out);
    AuditMisaligned(Items, Options.NearMissPx, Out);
}
} // namespace McpWidgetAudit

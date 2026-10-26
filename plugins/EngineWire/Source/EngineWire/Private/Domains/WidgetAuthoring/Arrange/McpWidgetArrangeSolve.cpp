// The solve of arrange_widgets (see McpWidgetArrange.h): the relations, then what holds besides them, in one Cassowary
// system over each widget's left, top, width and height.
#include "Domains/WidgetAuthoring/Arrange/McpWidgetArrange.h"

#include "Foundation/Solvers/McpCassowary.h"

namespace McpWidgetArrange
{
namespace
{
using McpCassowary::ERelation;

constexpr double ArrangeKeepSize = 100.0 * McpCassowary::Weak; // still far under a medium relation

void ArrangeAdd(McpCassowary::FSolver& Solver, TArray<TPair<int32, double>> Terms, double Constant, ERelation Relation, double Strength)
{
    McpCassowary::FConstraint C;
    C.Terms = MoveTemp(Terms);
    C.Constant = Constant;
    C.Relation = Relation;
    C.Strength = Strength;
    Solver.AddConstraint(C); // not required: a preferred constraint always goes in
}
} // namespace

bool Solve(const TArray<FRelation>& Relations, const TArray<FArrangeBox>& Current, const TArray<FVector2D>& MinSize, const FVector2D& Panel,
           TArray<FArrangeBox>& Out, int32& OutFailed)
{
    McpCassowary::FSolver Solver;
    for (int32 Index = 0; Index < Current.Num() * 4; ++Index)
    {
        Solver.AddVariable();
    }
    // Relations first, so a required one that cannot hold is named as the one that breaks the required ones before it.
    for (int32 Index = 0; Index < Relations.Num(); ++Index)
    {
        if (!AddRelation(Solver, Relations[Index], Panel))
        {
            OutFailed = Index;
            return false;
        }
    }
    using McpCassowary::Strong;
    using McpCassowary::Weak;
    for (int32 Widget = 0; Widget < Current.Num(); ++Widget)
    {
        const int32 L = 4 * Widget;
        const int32 T = L + 1;
        const int32 W = L + 2;
        const int32 H = L + 3;
        // Inside the panel and no smaller than its content needs (strong): a relation may still push it out.
        ArrangeAdd(Solver, {{W, 1.0}}, -FMath::Max(0.0, MinSize[Widget].X), ERelation::GreaterOrEqual, Strong);
        ArrangeAdd(Solver, {{H, 1.0}}, -FMath::Max(0.0, MinSize[Widget].Y), ERelation::GreaterOrEqual, Strong);
        ArrangeAdd(Solver, {{L, 1.0}}, 0.0, ERelation::GreaterOrEqual, Strong);
        ArrangeAdd(Solver, {{T, 1.0}}, 0.0, ERelation::GreaterOrEqual, Strong);
        ArrangeAdd(Solver, {{L, 1.0}, {W, 1.0}}, -Panel.X, ERelation::LessOrEqual, Strong);
        ArrangeAdd(Solver, {{T, 1.0}, {H, 1.0}}, -Panel.Y, ERelation::LessOrEqual, Strong);
        // Where and as large as it is now (weak): what no relation decides stays. Its size outweighs its place, so a
        // widget set in from an edge moves there rather than stretching to it, and a title does not shrink to pull the
        // widgets stacked under it up.
        ArrangeAdd(Solver, {{L, 1.0}}, -Current[Widget].Left, ERelation::Equal, Weak);
        ArrangeAdd(Solver, {{T, 1.0}}, -Current[Widget].Top, ERelation::Equal, Weak);
        ArrangeAdd(Solver, {{W, 1.0}}, -Current[Widget].Width, ERelation::Equal, ArrangeKeepSize);
        ArrangeAdd(Solver, {{H, 1.0}}, -Current[Widget].Height, ERelation::Equal, ArrangeKeepSize);
    }
    Out.Reset();
    for (int32 Widget = 0; Widget < Current.Num(); ++Widget)
    {
        FArrangeBox Box;
        Box.Left = Solver.Value(4 * Widget);
        Box.Top = Solver.Value(4 * Widget + 1);
        Box.Width = FMath::Max(0.0, Solver.Value(4 * Widget + 2));
        Box.Height = FMath::Max(0.0, Solver.Value(4 * Widget + 3));
        Out.Add(Box);
    }
    return true;
}
} // namespace McpWidgetArrange

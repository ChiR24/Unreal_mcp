// The relations of arrange_widgets (see McpWidgetArrange.h): read from JSON, turned into linear rows over each widget's
// left, top, width and height, given to the solver, and measured again on laid-out boxes.
#include "Domains/WidgetAuthoring/Arrange/McpWidgetArrange.h"

#include "Dom/JsonValue.h"
#include "Foundation/Solvers/McpCassowary.h"

namespace McpWidgetArrange
{
namespace
{
using McpCassowary::ERelation;
using McpCassowary::FConstraint;

enum class EArrangeLine : uint8
{
    Left,
    Right,
    CenterX,
    Top,
    Bottom,
    CenterY,
    Width,
    Height
};

// The terms of a widget's line: its variables are 4 * Widget + 0..3 (left, top, width, height).
void ArrangeLine(FConstraint& Row, int32 Widget, EArrangeLine Line, double Sign)
{
    const int32 Left = 4 * Widget;
    const bool bX = Line == EArrangeLine::Left || Line == EArrangeLine::Right || Line == EArrangeLine::CenterX;
    const double SizeShare = Line == EArrangeLine::Right || Line == EArrangeLine::Bottom ? 1.0 : (Line == EArrangeLine::CenterX || Line == EArrangeLine::CenterY ? 0.5 : 0.0);
    if (Line == EArrangeLine::Width || Line == EArrangeLine::Height)
    {
        Row.Terms.Add({Left + (Line == EArrangeLine::Width ? 2 : 3), Sign});
        return;
    }
    Row.Terms.Add({Left + (bX ? 0 : 1), Sign});
    if (SizeShare > 0.0)
    {
        Row.Terms.Add({Left + (bX ? 2 : 3), SizeShare * Sign});
    }
}

bool ArrangeLineOf(const FString& Type, EArrangeLine& Out)
{
    static const TPair<const TCHAR*, EArrangeLine> Lines[] = {
        {TEXT("alignLeft"), EArrangeLine::Left},       {TEXT("alignRight"), EArrangeLine::Right},     {TEXT("alignCenterX"), EArrangeLine::CenterX},
        {TEXT("alignTop"), EArrangeLine::Top},         {TEXT("alignBottom"), EArrangeLine::Bottom},   {TEXT("alignCenterY"), EArrangeLine::CenterY},
        {TEXT("sameWidth"), EArrangeLine::Width},      {TEXT("sameHeight"), EArrangeLine::Height}};
    for (const TPair<const TCHAR*, EArrangeLine>& Line : Lines)
    {
        if (Type.Equals(Line.Key, ESearchCase::IgnoreCase))
        {
            Out = Line.Value;
            return true;
        }
    }
    return false;
}

// The relation as rows "terms + constant (relation) 0".
TArray<FConstraint> ArrangeRows(const FRelation& R, const FVector2D& Panel)
{
    TArray<FConstraint> Rows;
    const auto Row = [&Rows, &R](ERelation Relation, double Constant) -> FConstraint&
    {
        FConstraint& C = Rows.AddDefaulted_GetRef();
        C.Relation = Relation;
        C.Constant = Constant;
        C.Strength = R.Strength;
        return C;
    };
    const ERelation Gap = R.bHasValue ? ERelation::Equal : ERelation::GreaterOrEqual;
    EArrangeLine Line = EArrangeLine::Left;
    if (R.Type == TEXT("leftOf") || R.Type == TEXT("rightOf") || R.Type == TEXT("above") || R.Type == TEXT("below"))
    {
        // "a leftOf b": b's left minus a's right is the gap.
        const bool bX = R.Type == TEXT("leftOf") || R.Type == TEXT("rightOf");
        const bool bFirstLeads = R.Type == TEXT("leftOf") || R.Type == TEXT("above");
        FConstraint& C = Row(Gap, -R.Value);
        ArrangeLine(C, R.Widgets[bFirstLeads ? 1 : 0], bX ? EArrangeLine::Left : EArrangeLine::Top, 1.0);
        ArrangeLine(C, R.Widgets[bFirstLeads ? 0 : 1], bX ? EArrangeLine::Right : EArrangeLine::Bottom, -1.0);
    }
    else if (ArrangeLineOf(R.Type, Line))
    {
        for (int32 Index = 1; Index < R.Widgets.Num(); ++Index)
        {
            FConstraint& C = Row(ERelation::Equal, 0.0);
            ArrangeLine(C, R.Widgets[0], Line, 1.0);
            ArrangeLine(C, R.Widgets[Index], Line, -1.0);
        }
    }
    else if (R.Type == TEXT("width") || R.Type == TEXT("height"))
    {
        ArrangeLine(Row(ERelation::Equal, -R.Value), R.Widgets[0], R.Type == TEXT("width") ? EArrangeLine::Width : EArrangeLine::Height, 1.0);
    }
    else if (R.Type == TEXT("inset"))
    {
        const bool bFar = R.Edge == TEXT("right") || R.Edge == TEXT("bottom");
        const bool bX = R.Edge == TEXT("left") || R.Edge == TEXT("right");
        const double Far = bX ? Panel.X : Panel.Y;
        ArrangeLine(Row(ERelation::Equal, bFar ? R.Value - Far : -R.Value), R.Widgets[0],
                    bFar ? (bX ? EArrangeLine::Right : EArrangeLine::Bottom) : (bX ? EArrangeLine::Left : EArrangeLine::Top), 1.0);
    }
    else if (R.Type == TEXT("center"))
    {
        if (R.Edge != TEXT("y"))
        {
            ArrangeLine(Row(ERelation::Equal, -0.5 * Panel.X), R.Widgets[0], EArrangeLine::CenterX, 1.0);
        }
        if (R.Edge != TEXT("x"))
        {
            ArrangeLine(Row(ERelation::Equal, -0.5 * Panel.Y), R.Widgets[0], EArrangeLine::CenterY, 1.0);
        }
    }
    return Rows;
}

double ArrangeStrength(const FString& Name, bool& bOk)
{
    bOk = true;
    if (Name.IsEmpty() || Name.Equals(TEXT("required"), ESearchCase::IgnoreCase)) return McpCassowary::Required;
    if (Name.Equals(TEXT("strong"), ESearchCase::IgnoreCase)) return McpCassowary::Strong;
    if (Name.Equals(TEXT("medium"), ESearchCase::IgnoreCase)) return McpCassowary::Medium;
    if (Name.Equals(TEXT("weak"), ESearchCase::IgnoreCase)) return McpCassowary::Weak;
    bOk = false;
    return 0.0;
}
} // namespace

bool ParseRelations(const TArray<TSharedPtr<FJsonValue>>& Json, TArray<FString>& Names, TArray<FRelation>& Out, FString& OutError)
{
    static const TArray<FString> Pairs = {TEXT("leftOf"), TEXT("rightOf"), TEXT("above"), TEXT("below")};
    static const TArray<FString> Edges = {TEXT("left"), TEXT("right"), TEXT("top"), TEXT("bottom")};
    for (int32 Index = 0; Index < Json.Num(); ++Index)
    {
        const TSharedPtr<FJsonObject> Object = Json[Index].IsValid() ? Json[Index]->AsObject() : nullptr;
        FRelation R;
        FString Widget;
        FString Of;
        FString Strength;
        const TArray<TSharedPtr<FJsonValue>>* Group = nullptr;
        if (!Object.IsValid() || !Object->TryGetStringField(TEXT("type"), R.Type))
        {
            OutError = FString::Printf(TEXT("relations[%d] needs a type."), Index);
            return false;
        }
        EArrangeLine Line;
        const bool bPair = Pairs.Contains(R.Type);
        const bool bGroup = ArrangeLineOf(R.Type, Line);
        Object->TryGetStringField(TEXT("widget"), Widget);
        Object->TryGetStringField(TEXT("of"), Of);
        Object->TryGetStringField(TEXT("strength"), Strength);
        Object->TryGetStringField(R.Type == TEXT("center") ? TEXT("axis") : TEXT("edge"), R.Edge);
        R.bHasValue = Object->TryGetNumberField(bPair ? TEXT("gap") : TEXT("value"), R.Value);
        TArray<FString> Members;
        if (bGroup && Object->TryGetArrayField(TEXT("widgets"), Group) && Group)
        {
            for (const TSharedPtr<FJsonValue>& Member : *Group)
            {
                Members.Add(Member.IsValid() ? Member->AsString() : FString());
            }
        }
        else
        {
            Members = bPair ? TArray<FString>{Widget, Of} : TArray<FString>{Widget};
        }
        bool bStrength = true;
        R.Strength = ArrangeStrength(Strength, bStrength);
        const bool bKnown = bPair || bGroup || R.Type == TEXT("width") || R.Type == TEXT("height") || R.Type == TEXT("inset") || R.Type == TEXT("center");
        const FString Problem = !bKnown ? FString::Printf(TEXT("type '%s' is not one of leftOf, rightOf, above, below, alignLeft, alignRight, alignTop, alignBottom, alignCenterX, alignCenterY, sameWidth, sameHeight, width, height, inset, center"), *R.Type)
            : Members.ContainsByPredicate([](const FString& Name) { return Name.IsEmpty(); }) ? FString(bPair ? TEXT("needs widget and of") : bGroup ? TEXT("needs widgets, two or more names") : TEXT("needs widget"))
            : bGroup && Members.Num() < 2 ? FString(TEXT("needs widgets, two or more names"))
            : !bStrength ? FString::Printf(TEXT("strength '%s' is not required, strong, medium or weak"), *Strength)
            : (R.Type == TEXT("width") || R.Type == TEXT("height") || R.Type == TEXT("inset")) && (!R.bHasValue || R.Value < 0.0) ? FString(TEXT("needs value, 0 or more"))
            : R.Type == TEXT("inset") && !Edges.Contains(R.Edge) ? FString(TEXT("needs edge: left, right, top or bottom"))
            : R.Type == TEXT("center") && !R.Edge.IsEmpty() && R.Edge != TEXT("x") && R.Edge != TEXT("y") && R.Edge != TEXT("both") ? FString(TEXT("takes axis x, y or both"))
            : FString();
        if (!Problem.IsEmpty())
        {
            OutError = FString::Printf(TEXT("relations[%d] %s."), Index, *Problem);
            return false;
        }
        for (const FString& Member : Members)
        {
            R.Widgets.Add(Names.AddUnique(Member));
        }
        R.Text = bPair ? FString::Printf(TEXT("%s %s %s%s"), *Members[0], *R.Type, *Members[1], R.bHasValue ? *FString::Printf(TEXT(" (gap %g)"), R.Value) : TEXT(""))
                       : FString::Printf(TEXT("%s %s%s%s"), *R.Type, *FString::Join(Members, TEXT(", ")), R.Edge.IsEmpty() ? TEXT("") : *(TEXT(" ") + R.Edge),
                                         R.bHasValue ? *FString::Printf(TEXT(" %g"), R.Value) : TEXT(""));
        Out.Add(MoveTemp(R));
    }
    return true;
}

bool AddRelation(McpCassowary::FSolver& Solver, const FRelation& Relation, const FVector2D& Panel)
{
    for (const FConstraint& Row : ArrangeRows(Relation, Panel))
    {
        if (!Solver.AddConstraint(Row))
        {
            return false;
        }
    }
    return true;
}

double Residual(const FRelation& Relation, const TArray<FArrangeBox>& Boxes, const FVector2D& Panel)
{
    double Worst = 0.0;
    for (const FConstraint& Row : ArrangeRows(Relation, Panel))
    {
        double Value = Row.Constant;
        for (const TPair<int32, double>& Term : Row.Terms)
        {
            const FArrangeBox& Box = Boxes[Term.Key / 4];
            const double Fields[4] = {Box.Left, Box.Top, Box.Width, Box.Height};
            Value += Term.Value * Fields[Term.Key % 4];
        }
        Worst = FMath::Max(Worst, Row.Relation == ERelation::Equal ? FMath::Abs(Value)
                                : Row.Relation == ERelation::GreaterOrEqual ? FMath::Max(0.0, -Value) : FMath::Max(0.0, Value));
    }
    return Worst;
}
} // namespace McpWidgetArrange

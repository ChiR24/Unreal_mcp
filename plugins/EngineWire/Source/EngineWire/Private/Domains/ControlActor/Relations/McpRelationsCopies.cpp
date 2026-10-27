// Copyright (c) 2024 EngineWire Contributors
//
// describe_relations' arrangements of copies of one prop (FItem::Shape). rows: 3 or more on one line along X or Y
// (within 5 cm of it), cut where a gap passes 4 footprints so copies far apart that happen to line up are no row, with
// the spacing and whether it is even; grids: rows of one count along X whose copies stand at the same X positions,
// stacked along Y (their rows and columns are not listed again as rows); rings: 5 or more copies around a common centre
// at one distance, evenly spread. All the copies of a prop are tried as one ring, so two rings of one prop read as none.

#include "Domains/ControlActor/Relations/McpRelations.h"

#include "Foundation/HandlerUtils/McpHandlerUtilsTransforms.h"

namespace McpRelations
{
namespace
{
constexpr double LineTolerance = 5.0;       // cm off a row's line, or between a grid's columns
constexpr double EvenShare = 0.05;          // a step within 5 percent of the mean is even
constexpr double TwoPi = 6.283185307179586; // UE_PI is missing in 5.0 and PI in 5.8

FVector Flat(const FItem& Item)
{
    const FVector Centre = Item.Box.GetCenter();
    return FVector(Centre.X, Centre.Y, 0.0);
}

// Runs of 3 or more on one line along Axis (0 X, 1 Y), ordered along it, cut where a gap passes MaxGap. The lines come
// in order across the axis.
TArray<TArray<int32>> Lines(const TArray<FItem>& Items, const TArray<int32>& Copies, int32 Axis, double MaxGap)
{
    const int32 Across = 1 - Axis;
    TArray<int32> Sorted = Copies;
    Sorted.Sort([&Items, Across](int32 A, int32 B) { return Flat(Items[A])[Across] < Flat(Items[B])[Across]; });
    TArray<TArray<int32>> Out;
    for (int32 Start = 0, End = 0; Start < Sorted.Num(); Start = End)
    {
        for (End = Start + 1;
             End < Sorted.Num() && Flat(Items[Sorted[End]])[Across] - Flat(Items[Sorted[Start]])[Across] <= LineTolerance; ++End)
        {
        }
        TArray<int32> Line(Sorted.GetData() + Start, End - Start);
        Line.Sort([&Items, Axis](int32 A, int32 B) { return Flat(Items[A])[Axis] < Flat(Items[B])[Axis]; });
        for (int32 RunStart = 0, Index = 1; Index <= Line.Num(); ++Index)
        {
            if (Index == Line.Num() || Flat(Items[Line[Index]])[Axis] - Flat(Items[Line[Index - 1]])[Axis] > MaxGap)
            {
                if (Index - RunStart >= 3)
                {
                    Out.Emplace(Line.GetData() + RunStart, Index - RunStart);
                }
                RunStart = Index;
            }
        }
    }
    return Out;
}

// The mean step along Axis between consecutive members, and whether every step is within EvenShare of it.
double Spacing(const TArray<FItem>& Items, const TArray<int32>& Members, int32 Axis, bool& bOutEven)
{
    const double Mean = (Flat(Items[Members.Last()])[Axis] - Flat(Items[Members[0]])[Axis]) / (Members.Num() - 1);
    bOutEven = true;
    for (int32 Index = 1; Index < Members.Num(); ++Index)
    {
        const double Step = Flat(Items[Members[Index]])[Axis] - Flat(Items[Members[Index - 1]])[Axis];
        bOutEven &= FMath::Abs(Step - Mean) <= FMath::Max(2.0, EvenShare * Mean);
    }
    return Mean;
}

TSharedPtr<FJsonValue> RowJson(const TArray<FItem>& Items, const TArray<int32>& Members, int32 Axis)
{
    bool bEven = false;
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetArrayField(TEXT("members"), MemberNames(Items, Members));
    Row->SetStringField(TEXT("along"), Axis == 0 ? TEXT("x") : TEXT("y"));
    Row->SetNumberField(TEXT("spacing"), FMath::RoundToDouble(Spacing(Items, Members, Axis, bEven)));
    Row->SetBoolField(TEXT("even"), bEven);
    return MakeShared<FJsonValueObject>(Row);
}

bool SameColumns(const TArray<FItem>& Items, const TArray<int32>& A, const TArray<int32>& B)
{
    for (int32 Index = 0; Index < A.Num() && A.Num() == B.Num(); ++Index)
    {
        if (FMath::Abs(Flat(Items[A[Index]]).X - Flat(Items[B[Index]]).X) > LineTolerance)
        {
            return false;
        }
    }
    return A.Num() == B.Num();
}

void AddRowsAndGrids(const TArray<FItem>& Items, const TArray<int32>& Copies, FPatterns& Out)
{
    const double MaxGap = FMath::Max(400.0, 4.0 * Footprint(Items[Copies[0]]));
    const TArray<TArray<int32>> AlongX = Lines(Items, Copies, 0, MaxGap);
    TArray<bool> InGrid;
    InGrid.Init(false, AlongX.Num());
    TSet<int32> GridCopies;
    for (int32 First = 0; First < AlongX.Num(); ++First)
    {
        TArray<int32> Stack;
        Stack.Add(First);
        for (int32 Next = First + 1; Next < AlongX.Num() && !InGrid[First]; ++Next)
        {
            const double Step = Flat(Items[AlongX[Next][0]]).Y - Flat(Items[AlongX[Stack.Last()][0]]).Y;
            if (!InGrid[Next] && Step <= MaxGap && SameColumns(Items, AlongX[First], AlongX[Next]))
            {
                Stack.Add(Next);
            }
        }
        if (Stack.Num() < 2)
        {
            continue;
        }
        TArray<int32> Members, Heads;
        for (const int32 Line : Stack)
        {
            InGrid[Line] = true;
            Members.Append(AlongX[Line]);
            Heads.Add(AlongX[Line][0]);
        }
        GridCopies.Append(Members);
        bool bEvenX = false, bEvenY = false;
        const double StepX = Spacing(Items, AlongX[First], 0, bEvenX);
        const double StepY = Spacing(Items, Heads, 1, bEvenY);
        const TArray<TSharedPtr<FJsonValue>> Steps = {MakeShared<FJsonValueNumber>(FMath::RoundToDouble(StepX)),
                                                      MakeShared<FJsonValueNumber>(FMath::RoundToDouble(StepY))};
        TSharedPtr<FJsonObject> Grid = MakeShared<FJsonObject>();
        Grid->SetArrayField(TEXT("members"), MemberNames(Items, Members));
        Grid->SetNumberField(TEXT("columns"), AlongX[First].Num());
        Grid->SetNumberField(TEXT("rows"), Stack.Num());
        Grid->SetArrayField(TEXT("spacing"), Steps);
        Grid->SetBoolField(TEXT("even"), bEvenX && bEvenY);
        Out.Grids.Add(MakeShared<FJsonValueObject>(Grid));
    }
    for (int32 Line = 0; Line < AlongX.Num(); ++Line)
    {
        if (!InGrid[Line])
        {
            Out.Rows.Add(RowJson(Items, AlongX[Line], 0));
        }
    }
    for (const TArray<int32>& Line : Lines(Items, Copies, 1, MaxGap))
    {
        if (Line.ContainsByPredicate([&GridCopies](int32 Copy) { return !GridCopies.Contains(Copy); }))
        {
            Out.Rows.Add(RowJson(Items, Line, 1));
        }
    }
}

// Whether Members stand evenly around their mean centre at one distance; OutCentre and OutRadius say where.
bool IsRing(const TArray<FItem>& Items, const TArray<int32>& Members, FVector& OutCentre, double& OutRadius)
{
    OutCentre = FVector::ZeroVector;
    for (const int32 Index : Members)
    {
        OutCentre += Flat(Items[Index]) / Members.Num();
    }
    TArray<double> Angles;
    OutRadius = 0.0;
    for (const int32 Index : Members)
    {
        const FVector Offset = Flat(Items[Index]) - OutCentre;
        OutRadius += Offset.Size() / Members.Num();
        Angles.Add(FMath::Atan2(Offset.Y, Offset.X));
    }
    Angles.Sort();
    const double Step = TwoPi / Members.Num();
    bool bRing = OutRadius > 1.0;
    for (int32 Index = 0; Index < Members.Num() && bRing; ++Index)
    {
        const double Gap = (Index + 1 < Angles.Num() ? Angles[Index + 1] : Angles[0] + TwoPi) - Angles[Index];
        const double Distance = (Flat(Items[Members[Index]]) - OutCentre).Size();
        bRing = FMath::Abs(Gap - Step) <= 0.25 * Step && FMath::Abs(Distance - OutRadius) <= FMath::Max(2.0, EvenShare * OutRadius);
    }
    return bRing;
}
} // namespace

void FindArrangements(const TArray<FItem>& Items, const TArray<int32>& Copies, const TSet<FString>& Kinds, FPatterns& Out)
{
    if ((Kinds.Num() == 0 || Kinds.Contains(TEXT("rows"))) && Copies.Num() >= 3)
    {
        AddRowsAndGrids(Items, Copies, Out);
    }
    FVector Centre = FVector::ZeroVector;
    double Radius = 0.0;
    if ((Kinds.Num() == 0 || Kinds.Contains(TEXT("rings"))) && Copies.Num() >= 5 && IsRing(Items, Copies, Centre, Radius))
    {
        for (const int32 Index : Copies)
        {
            Centre.Z += Items[Index].Box.GetCenter().Z / Copies.Num(); // judged on the ground plane; reported at their height
        }
        TSharedPtr<FJsonObject> Ring = MakeShared<FJsonObject>();
        Ring->SetArrayField(TEXT("members"), MemberNames(Items, Copies));
        Ring->SetArrayField(TEXT("center"), McpHandlerUtils::VectorToJsonArray(Centre));
        Ring->SetNumberField(TEXT("radius"), FMath::RoundToDouble(Radius));
        Out.Rings.Add(MakeShared<FJsonValueObject>(Ring));
    }
}
} // namespace McpRelations

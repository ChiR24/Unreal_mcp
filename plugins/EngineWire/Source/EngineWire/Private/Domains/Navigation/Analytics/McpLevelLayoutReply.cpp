// The reply of analyze_level_layout (see McpLevelLayout.h): a summary a reader takes in at once, and rows per space and
// door for the details.
#include "Domains/Navigation/Analytics/McpLevelLayout.h"

#include "Algo/Reverse.h"
#include "CollisionQueryParams.h"
#include "Dom/JsonValue.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Misc/EngineVersionComparison.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
#include "Engine/HitResult.h" // 5.0 declares FHitResult in EngineTypes.h
#endif

namespace McpLevelLayout
{
namespace
{
constexpr double LayoutEyeCm = 160.0;
constexpr double LayoutSightCm = 5000.0;
constexpr int32 LayoutSightRays = 16;
constexpr double LayoutTwoPi = 6.283185307179586; // PI is gone in 5.8 and UE_PI missing in 5.0
constexpr int32 LayoutPathPoints = 40;
constexpr double LayoutChokeShare = 0.01; // a space carrying fewer of the routes is no choke
constexpr int32 LayoutMaxRows = 100;      // spaces and doors listed (a cluttered level makes hundreds)
constexpr int32 LayoutMaxNames = 50;      // names in chokes, deadEnds and unreachable
constexpr int32 LayoutSummaryNames = 8;   // dead ends the summary names

TArray<TSharedPtr<FJsonValue>> LayoutPoint(const FVector& Value)
{
    return {MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Value.X)), MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Value.Y)),
            MakeShared<FJsonValueNumber>(FMath::RoundToDouble(Value.Z))};
}

double LayoutRound(double Value, double Step) { return FMath::RoundToDouble(Value / Step) * Step; }

FString LayoutName(const FLayout& Layout, int32 Space)
{
    int32 Rooms = 0;
    for (const FSpace& Each : Layout.Spaces)
    {
        Rooms += Each.bRoom ? 1 : 0;
    }
    return Layout.Spaces[Space].bRoom ? FString::Printf(TEXT("Room %d"), Space + 1) : FString::Printf(TEXT("Corridor %d"), Space - Rooms + 1);
}

TArray<TSharedPtr<FJsonValue>> LayoutNames(const FLayout& Layout, const TArray<int32>& Spaces)
{
    TArray<TSharedPtr<FJsonValue>> Names;
    for (int32 Index = 0; Index < Spaces.Num() && Index < LayoutMaxNames; ++Index)
    {
        Names.Add(MakeShared<FJsonValueString>(LayoutName(Layout, Spaces[Index])));
    }
    return Names;
}

// The longest of LayoutSightRays level traces at eye height from a sample (LayoutSightCm when nothing is hit).
double LayoutSight(UWorld* World, const FVector& Ground)
{
    const FVector Eye = Ground + FVector(0.0, 0.0, LayoutEyeCm);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(McpLevelLayoutSight), false);
    double Longest = 0.0;
    for (int32 Ray = 0; Ray < LayoutSightRays; ++Ray)
    {
        const double Angle = LayoutTwoPi * Ray / LayoutSightRays;
        FHitResult Hit;
        const bool bHit = World->LineTraceSingleByChannel(
            Hit, Eye, Eye + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * LayoutSightCm, ECC_Visibility, Params);
        Longest = FMath::Max(Longest, bHit ? double(Hit.Distance) : LayoutSightCm);
    }
    return Longest;
}

// The walk from the start to GoalNode: its samples in order, its length and the spaces it crosses.
TSharedPtr<FJsonObject> LayoutPath(const FGraph& Graph, const FLayout& Layout, const TArray<int32>& Parent, int32 GoalNode,
                                   FString& OutText)
{
    TSharedPtr<FJsonObject> Path = MakeShared<FJsonObject>();
    Path->SetBoolField(TEXT("reachable"), Parent[GoalNode] != INDEX_NONE);
    if (Parent[GoalNode] == INDEX_NONE)
    {
        OutText = TEXT("cannot walk there");
        return Path;
    }
    TArray<int32> Nodes = {GoalNode};
    while (Parent[Nodes.Last()] != Nodes.Last())
    {
        Nodes.Add(Parent[Nodes.Last()]);
    }
    Algo::Reverse(Nodes);
    double Length = 0.0;
    TArray<int32> Crossed;
    TArray<TSharedPtr<FJsonValue>> Points;
    for (int32 Index = 0; Index < Nodes.Num(); ++Index)
    {
        Length += Index > 0 ? FVector::Dist(Graph.Points[Nodes[Index - 1]], Graph.Points[Nodes[Index]]) : 0.0;
        if (Crossed.Num() == 0 || Crossed.Last() != Layout.SpaceOf[Nodes[Index]])
        {
            Crossed.Add(Layout.SpaceOf[Nodes[Index]]);
        }
        const int32 Every = FMath::Max(1, Nodes.Num() / LayoutPathPoints);
        if (Index % Every == 0 || Index == Nodes.Num() - 1)
        {
            Points.Add(MakeShared<FJsonValueArray>(LayoutPoint(Graph.Points[Nodes[Index]])));
        }
    }
    Path->SetNumberField(TEXT("lengthCm"), FMath::RoundToDouble(Length));
    Path->SetArrayField(TEXT("spaces"), LayoutNames(Layout, Crossed));
    Path->SetArrayField(TEXT("points"), Points);
    TArray<FString> Names;
    for (const int32 Space : Crossed)
    {
        Names.Add(LayoutName(Layout, Space));
    }
    OutText = FString::Printf(TEXT("%.0f m through %s"), Length / 100.0, *FString::Join(Names, TEXT(", ")));
    return Path;
}
} // namespace

FString WriteReply(UWorld* World, const FGraph& Graph, const FLayout& Layout, const TArray<int32>& Parent, int32 StartNode,
                   int32 GoalNode, const FString& StartName, const FString& GoalName, const TSharedPtr<FJsonObject>& Result)
{
    const double Cell = Graph.CellCm;
    TArray<bool> Reached;
    Reached.Init(false, Layout.Spaces.Num());
    for (int32 Node = 0; Node < Parent.Num(); ++Node)
    {
        Reached[Layout.SpaceOf[Node]] |= Parent[Node] != INDEX_NONE;
    }
    TArray<TSharedPtr<FJsonValue>> SpaceRows;
    TArray<int32> DeadEnds;
    TArray<int32> Unreachable;
    TArray<int32> Chokes;
    int32 Rooms = 0;
    double Area = 0.0;
    for (int32 Space = 0; Space < Layout.Spaces.Num(); ++Space)
    {
        const FSpace& Each = Layout.Spaces[Space];
        const double SpaceArea = Each.Floor * Cell * Cell / 10000.0;
        Rooms += Each.bRoom ? 1 : 0;
        Area += SpaceArea;
        DeadEnds.Add(Each.Neighbours.Num() == 1 ? Space : INDEX_NONE);
        Unreachable.Add(StartNode != INDEX_NONE && !Reached[Space] ? Space : INDEX_NONE);
        Chokes.Add(Each.PassShare >= LayoutChokeShare ? Space : INDEX_NONE);
        if (Space >= LayoutMaxRows)
        {
            continue; // counted, not listed (the largest come first)
        }
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("name"), LayoutName(Layout, Space));
        Row->SetStringField(TEXT("kind"), Each.bRoom ? TEXT("room") : TEXT("corridor"));
        Row->SetArrayField(TEXT("center"), LayoutPoint(Each.Center));
        Row->SetNumberField(TEXT("areaM2"), LayoutRound(SpaceArea, 0.1));
        Row->SetNumberField(TEXT("widthCm"), FMath::RoundToDouble((2 * Each.Height + 1) * Cell));
        Row->SetArrayField(TEXT("neighbours"), LayoutNames(Layout, Each.Neighbours));
        Row->SetBoolField(TEXT("deadEnd"), Each.Neighbours.Num() == 1);
        Row->SetNumberField(TEXT("passThroughShare"), LayoutRound(Each.PassShare, 0.001));
        Row->SetNumberField(TEXT("sightlineCm"), FMath::RoundToDouble(LayoutSight(World, Graph.Points[Each.Deepest])));
        if (StartNode != INDEX_NONE)
        {
            Row->SetBoolField(TEXT("reachable"), Reached[Space]);
        }
        SpaceRows.Add(MakeShared<FJsonValueObject>(Row));
    }
    for (TArray<int32>* List : {&DeadEnds, &Unreachable, &Chokes})
    {
        List->Remove(INDEX_NONE);
    }
    // The spaces most routes between the others run through; of two equal shares the narrower is the choke.
    Chokes.Sort([&Layout](int32 L, int32 R)
                {
                    const FSpace& A = Layout.Spaces[L];
                    const FSpace& B = Layout.Spaces[R];
                    return FMath::Abs(A.PassShare - B.PassShare) > 1e-6 ? A.PassShare > B.PassShare : A.Height < B.Height;
                });
    // The most used doors first, as many as spaces are listed.
    TArray<const FDoor*> Doors;
    for (const FDoor& Door : Layout.Doors)
    {
        Doors.Add(&Door);
    }
    Doors.StableSort([](const FDoor& L, const FDoor& R) { return L.Share > R.Share; });
    TArray<TSharedPtr<FJsonValue>> DoorRows;
    for (int32 Index = 0; Index < Doors.Num() && Index < LayoutMaxRows; ++Index)
    {
        const FDoor& Door = *Doors[Index];
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetArrayField(TEXT("between"), LayoutNames(Layout, {Door.A, Door.B}));
        Row->SetArrayField(TEXT("location"), LayoutPoint(Door.Location));
        Row->SetNumberField(TEXT("widthCm"), FMath::RoundToDouble(Door.WidthCells * Cell));
        Row->SetNumberField(TEXT("share"), LayoutRound(Door.Share, 0.001));
        DoorRows.Add(MakeShared<FJsonValueObject>(Row));
    }

    TArray<FString> Lines;
    Lines.Add(FString::Printf(TEXT("%d room(s) and %d corridor(s) over %.0f m2 of nav mesh (%d samples every %.0f cm)."), Rooms,
                              Layout.Spaces.Num() - Rooms, Area, Graph.Points.Num(), Cell));
    if (Chokes.Num() > 0)
    {
        Lines.Add(FString::Printf(TEXT("Top choke: %s, on %.0f%% of the routes between the other spaces."),
                                  *LayoutName(Layout, Chokes[0]), Layout.Spaces[Chokes[0]].PassShare * 100.0));
    }
    TArray<FString> DeadNames;
    for (int32 Index = 0; Index < DeadEnds.Num() && Index < LayoutSummaryNames; ++Index)
    {
        DeadNames.Add(LayoutName(Layout, DeadEnds[Index]));
    }
    if (DeadEnds.Num() > LayoutSummaryNames)
    {
        DeadNames.Add(FString::Printf(TEXT("%d more"), DeadEnds.Num() - LayoutSummaryNames));
    }
    Lines.Add(FString::Printf(TEXT("Dead ends: %s. Loops: %d."), DeadNames.Num() > 0 ? *FString::Join(DeadNames, TEXT(", ")) : TEXT("none"), Layout.Loops));
    if (StartNode != INDEX_NONE && GoalNode != INDEX_NONE)
    {
        FString Walk;
        const TSharedPtr<FJsonObject> Path = LayoutPath(Graph, Layout, Parent, GoalNode, Walk);
        Path->SetStringField(TEXT("from"), StartName);
        Path->SetStringField(TEXT("to"), GoalName);
        Result->SetObjectField(TEXT("path"), Path);
        Lines.Add(FString::Printf(TEXT("%s to %s: %s."), *StartName, *GoalName, *Walk));
    }
    if (StartNode != INDEX_NONE)
    {
        Result->SetArrayField(TEXT("unreachable"), LayoutNames(Layout, Unreachable));
        Lines.Add(Unreachable.Num() > 0 ? FString::Printf(TEXT("%d space(s) cannot be walked to from %s."), Unreachable.Num(), *StartName)
                                        : FString::Printf(TEXT("Every space can be walked to from %s."), *StartName));
    }
    if (Layout.Spaces.Num() > LayoutMaxRows || Layout.Doors.Num() > LayoutMaxRows)
    {
        Lines.Add(FString::Printf(TEXT("spaces[] lists the %d largest of %d and doors[] the %d most used of %d."), SpaceRows.Num(),
                                  Layout.Spaces.Num(), DoorRows.Num(), Layout.Doors.Num()));
    }
    Lines.Add(Graph.Note.TrimStartAndEnd());
    Result->SetArrayField(TEXT("spaces"), SpaceRows);
    Result->SetArrayField(TEXT("doors"), DoorRows);
    Result->SetArrayField(TEXT("chokes"), LayoutNames(Layout, Chokes));
    Result->SetArrayField(TEXT("deadEnds"), LayoutNames(Layout, DeadEnds));
    Result->SetNumberField(TEXT("loops"), Layout.Loops);
    Result->SetNumberField(TEXT("samples"), Graph.Points.Num());
    Result->SetNumberField(TEXT("cellCm"), Cell);
    Lines.Remove(FString());
    return FString::Join(Lines, TEXT(" "));
}
} // namespace McpLevelLayout

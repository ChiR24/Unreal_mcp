// The nav mesh as a graph of samples (analyze_level_layout; see McpLevelLayout.h).
#include "Domains/Navigation/Analytics/McpLevelLayout.h"

#include "Engine/World.h"
#include "NavigationSystem.h"

namespace McpLevelLayout
{
namespace
{
constexpr double LayoutFloorStepCm = 300.0; // columns are probed from this many cm apart, so stacked floors each land
constexpr double LayoutSameFloorCm = 100.0; // two hits in one column closer than this are one sample
constexpr double LayoutStepCm = 60.0;       // a rise any walker takes between neighbouring cells, on top of a 45-degree slope
constexpr double LayoutMaxProbes = 250000.0; // nav mesh projections per call (columns times heights)
constexpr int32 LayoutMaxLevels = 200;       // heights probed per column: 600 m of floors

constexpr int32 LayoutMinIsland = 4;        // a part of the graph with fewer samples is a speck (a crate's top, a ledge)

double LayoutColumns(const FVector& Size, double Cell)
{
    return (FMath::FloorToDouble(Size.X / Cell) + 1.0) * (FMath::FloorToDouble(Size.Y / Cell) + 1.0);
}

// Graph without its specks: no space anyone walks, they would only crowd the spaces and the unreachable list.
void LayoutDropSpecks(FGraph& Graph)
{
    const int32 Num = Graph.Points.Num();
    TArray<int32> Part; // each sample's part, named by its first sample
    Part.Init(INDEX_NONE, Num);
    TMap<int32, int32> PartSize;
    for (int32 Seed = 0; Seed < Num; ++Seed)
    {
        if (Part[Seed] != INDEX_NONE)
        {
            continue;
        }
        TArray<int32> Queue = {Seed};
        Part[Seed] = Seed;
        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            for (const int32 Next : Graph.Adjacency[Queue[Head]])
            {
                if (Part[Next] == INDEX_NONE)
                {
                    Part[Next] = Seed;
                    Queue.Add(Next);
                }
            }
        }
        PartSize.Add(Seed, Queue.Num());
    }
    TArray<int32> Kept; // new index per sample, INDEX_NONE for a speck's
    TArray<FVector> Points;
    TArray<FIntPoint> Cells;
    TArray<double> Floor;
    for (int32 Node = 0; Node < Num; ++Node)
    {
        const bool bKeep = PartSize[Part[Node]] >= LayoutMinIsland;
        Kept.Add(bKeep ? Points.Add(Graph.Points[Node]) : INDEX_NONE);
        if (bKeep)
        {
            Cells.Add(Graph.Cells[Node]);
            Floor.Add(Graph.Floor[Node]);
        }
    }
    TArray<TArray<int32>> Adjacency;
    Adjacency.SetNum(Points.Num());
    for (int32 Node = 0; Node < Num; ++Node)
    {
        if (Kept[Node] == INDEX_NONE)
        {
            continue;
        }
        for (const int32 Next : Graph.Adjacency[Node])
        {
            Adjacency[Kept[Node]].Add(Kept[Next]); // a kept sample's neighbours are in its part, so kept too
        }
    }
    if (Points.Num() < Num)
    {
        Graph.Note += FString::Printf(TEXT(" Left out %d sample(s) on islands of under %d (a crate's top, a ledge)."),
                                      Num - Points.Num(), LayoutMinIsland);
    }
    Graph.Points = MoveTemp(Points);
    Graph.Cells = MoveTemp(Cells);
    Graph.Floor = MoveTemp(Floor);
    Graph.Adjacency = MoveTemp(Adjacency);
}
} // namespace

bool Sample(UWorld* World, const FSampleSpec& Spec, FGraph& Out, FString& OutCode, FString& OutMessage)
{
    UNavigationSystemV1* Nav = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
    ANavigationData* Data = Nav ? Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
    if (!Data)
    {
        OutCode = TEXT("NAV_DATA_MISSING");
        OutMessage = TEXT("The level has no navigation data: put a Nav Mesh Bounds Volume around the walkable area and build "
                          "it (manage_ai rebuild_navigation).");
        return false;
    }
    if (Nav->IsNavigationBuildInProgress())
    {
        OutCode = TEXT("NAV_BUILD_IN_PROGRESS");
        OutMessage = TEXT("The nav mesh is still building, so its layout would be stale: ask again once it is built.");
        return false;
    }
    const FVector Size = Spec.Area.GetSize();
    const int32 Levels = FMath::Max(1, FMath::CeilToInt(Size.Z / LayoutFloorStepCm));
    if (Levels > LayoutMaxLevels)
    {
        OutCode = TEXT("INVALID_ARGUMENT");
        OutMessage = FString::Printf(TEXT("The area read is %.0f m tall, past the %.0f m read floor by floor: give center and "
                                          "extent for a lower box."), Size.Z / 100.0, LayoutMaxLevels * LayoutFloorStepCm / 100.0);
        return false;
    }
    double Cell = FMath::Max(Spec.CellCm, 10.0);
    while ((LayoutColumns(Size, Cell) > Spec.MaxNodes || LayoutColumns(Size, Cell) * Levels > LayoutMaxProbes) &&
           LayoutColumns(Size, Cell) > 1.0)
    {
        Cell = FMath::CeilToDouble(Cell * 1.25);
    }
    if (Cell > Spec.CellCm)
    {
        Out.Note = FString::Printf(TEXT("Sampled every %.0f cm: cellCm %.0f over this area would pass maxNodes %d or take too long."),
                                   Cell, Spec.CellCm, Spec.MaxNodes);
    }
    Out.CellCm = Cell;
    Out.GridSize = FIntPoint(FMath::FloorToInt(Size.X / Cell) + 1, FMath::FloorToInt(Size.Y / Cell) + 1);
    const FVector Extent(Cell * 0.5, Cell * 0.5, LayoutFloorStepCm * 0.5 + 10.0);
    TMap<FIntPoint, TArray<int32>> Columns;
    for (int32 X = 0; X < Out.GridSize.X; ++X)
    {
        for (int32 Y = 0; Y < Out.GridSize.Y; ++Y)
        {
            const double CX = Spec.Area.Min.X + X * Cell;
            const double CY = Spec.Area.Min.Y + Y * Cell;
            for (int32 Level = 0; Level < Levels && Out.Points.Num() < Spec.MaxNodes; ++Level)
            {
                const double Z = FMath::Max(Spec.Area.Max.Z - (Level + 0.5) * LayoutFloorStepCm, Spec.Area.Min.Z);
                FNavLocation Hit;
                if (!Nav->ProjectPointToNavigation(FVector(CX, CY, Z), Hit, Extent, Data) ||
                    FMath::Abs(Hit.Location.X - CX) > Cell * 0.5 || FMath::Abs(Hit.Location.Y - CY) > Cell * 0.5)
                {
                    continue; // nothing walkable in this cell at this height (the nearest mesh is a neighbour's)
                }
                TArray<int32>& Column = Columns.FindOrAdd(FIntPoint(X, Y));
                if (Column.ContainsByPredicate([&Out, &Hit](int32 Seen)
                                               { return FMath::Abs(Out.Points[Seen].Z - Hit.Location.Z) < LayoutSameFloorCm; }))
                {
                    continue;
                }
                Column.Add(Out.Points.Add(Hit.Location));
                Out.Cells.Add(FIntPoint(X, Y));
                // A cell whose centre is on the mesh is floor; one reached only by snapping to the rim, about half.
                Out.Floor.Add(FVector::Dist2D(Hit.Location, FVector(CX, CY, 0.0)) < 1.0 ? 1.0 : 0.5);
            }
        }
    }
    if (Out.Points.Num() >= Spec.MaxNodes)
    {
        Out.Note += FString::Printf(TEXT(" The samples stopped at maxNodes %d: part of the area was not read."), Spec.MaxNodes);
    }
    // Each pair of neighbouring samples once (four of the eight directions), joined when the nav mesh runs straight between them.
    const FIntPoint Half[4] = {FIntPoint(1, 0), FIntPoint(1, 1), FIntPoint(0, 1), FIntPoint(-1, 1)};
    Out.Adjacency.SetNum(Out.Points.Num());
    for (int32 Node = 0; Node < Out.Points.Num(); ++Node)
    {
        for (const FIntPoint& Step : Half)
        {
            const TArray<int32>* Column = Columns.Find(Out.Cells[Node] + Step);
            if (!Column)
            {
                continue;
            }
            for (const int32 Next : *Column)
            {
                const FVector& A = Out.Points[Node];
                const FVector& B = Out.Points[Next];
                FVector Blocked;
                // A ray starting on the mesh's very rim (where a sample snapped to it) reads as blocked at once, so a
                // pair is joined when either way along it is open.
                if (FMath::Abs(A.Z - B.Z) <= LayoutStepCm + FVector::Dist2D(A, B) &&
                    (!UNavigationSystemV1::NavigationRaycast(World, A, B, Blocked) ||
                     !UNavigationSystemV1::NavigationRaycast(World, B, A, Blocked)))
                {
                    Out.Adjacency[Node].Add(Next);
                    Out.Adjacency[Next].Add(Node);
                }
            }
        }
    }
    LayoutDropSpecks(Out);
    return true;
}
} // namespace McpLevelLayout

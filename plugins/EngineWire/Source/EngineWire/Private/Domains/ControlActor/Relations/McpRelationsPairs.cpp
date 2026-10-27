// Copyright (c) 2024 EngineWire Contributors
//
// describe_relations' pairwise relations. on: a bottom resting on a top (down-traces from the centre and four inset
// corners of the footprint; the actor hit most often within 3 cm); mounted: held from the side by a neighbour (the
// placement checks' own pair test, McpPlacement::HangsOn); inside: within another's bounds, the tightest such
// container; nextTo: sides within 2 cm side by side, on footprints turned with the actors; faces: the line ahead of its
// front (X axis) meets another or points within 20 degrees of its centre, the nearest such that is not a copy of it (a
// row of crates all "faced" the next crate). A pair that rests or hangs one on the other is neither nextTo nor faces.
// Candidates come from a uniform grid over the ground plane, so a level of hundreds of actors checks only neighbours.

#include "Domains/ControlActor/Relations/McpRelations.h"

#include "Domains/ControlActor/Placement/McpPlacementMount.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/EngineVersionComparison.h"
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1)
#include "Engine/HitResult.h" // 5.0 declares FHitResult in EngineTypes.h
#endif

namespace McpRelations
{
namespace
{
constexpr double TouchGap = 2.0;         // cm between faces that touch
constexpr double FacingGrace = 0.34907;  // 20 degrees in radians
constexpr int32 MaxCellsPerItem = 256;   // an item wider than this many cells is checked against every item

// The items whose boxes reach each cell of a grid over X and Y; an item too wide for it is near everything.
struct FGrid
{
    double Cell = 200.0;
    TMap<FIntPoint, TArray<int32>> Cells;
    TArray<int32> Wide;

    FIntPoint Key(double X, double Y) const
    {
        return FIntPoint(FMath::FloorToInt(X / Cell), FMath::FloorToInt(Y / Cell));
    }
    void Add(int32 Index, const FBox& Box)
    {
        const FIntPoint Lo = Key(Box.Min.X, Box.Min.Y);
        const FIntPoint Hi = Key(Box.Max.X, Box.Max.Y);
        if ((Hi.X - Lo.X + 1) * (Hi.Y - Lo.Y + 1) > MaxCellsPerItem)
        {
            Wide.Add(Index);
            return;
        }
        for (int32 X = Lo.X; X <= Hi.X; ++X)
        {
            for (int32 Y = Lo.Y; Y <= Hi.Y; ++Y)
            {
                Cells.FindOrAdd(FIntPoint(X, Y)).AddUnique(Index);
            }
        }
    }
    TSet<int32> Near(const FBox& Box) const
    {
        TSet<int32> Out(Wide);
        const FIntPoint Lo = Key(Box.Min.X, Box.Min.Y);
        const FIntPoint Hi = Key(Box.Max.X, Box.Max.Y);
        for (int32 X = Lo.X; X <= Hi.X && (X - Lo.X) < 64; ++X)
        {
            for (int32 Y = Lo.Y; Y <= Hi.Y && (Y - Lo.Y) < 64; ++Y)
            {
                if (const TArray<int32>* Found = Cells.Find(FIntPoint(X, Y)))
                {
                    Out.Append(*Found);
                }
            }
        }
        return Out;
    }
};

// Cells twice the median footprint, so an item and its neighbours share a few cells.
FGrid BuildGrid(const TArray<FItem>& Items)
{
    TArray<double> Sizes;
    for (const FItem& Item : Items)
    {
        const FVector Size = Item.Box.GetSize();
        Sizes.Add(FMath::Max(Size.X, Size.Y));
    }
    Sizes.Sort();
    FGrid Grid;
    Grid.Cell = FMath::Clamp(Sizes.Num() > 0 ? 2.0 * Sizes[Sizes.Num() / 2] : 200.0, 100.0, 1000.0);
    for (int32 Index = 0; Index < Items.Num(); ++Index)
    {
        Grid.Add(Index, Items[Index].Box);
    }
    return Grid;
}

// What A rests on: the item the most of five short down-traces from its bottom strike within SupportTolerance.
int32 FindSupport(UWorld* World, const TArray<FItem>& Items, const TMap<const AActor*, int32>& IndexOf, int32 A, int32& OutHits)
{
    const FBox& Box = Items[A].Box;
    const FVector Centre = Box.GetCenter();
    const FVector Extent = Box.GetExtent();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(McpRelationsSupport), false, Items[A].Actor);
    FCollisionObjectQueryParams Solids;
    Solids.AddObjectTypesToQuery(ECC_WorldStatic);
    Solids.AddObjectTypesToQuery(ECC_WorldDynamic);
    Solids.AddObjectTypesToQuery(ECC_PhysicsBody);
    TMap<int32, int32> Votes;
    static const FVector2D Points[] = {{0.0, 0.0}, {0.6, 0.6}, {-0.6, 0.6}, {0.6, -0.6}, {-0.6, -0.6}};
    for (const FVector2D& Point : Points)
    {
        const FVector Start(Centre.X + Point.X * Extent.X, Centre.Y + Point.Y * Extent.Y, Box.Min.Z + SupportTolerance);
        TArray<FHitResult> Hits;
        World->LineTraceMultiByObjectType(Hits, Start, Start - FVector(0.0, 0.0, 2.0 * SupportTolerance), Solids, Params);
        for (const FHitResult& Hit : Hits)
        {
            const int32* Other = Hit.GetActor() ? IndexOf.Find(Hit.GetActor()) : nullptr;
            if (Other && *Other != A)
            {
                ++Votes.FindOrAdd(*Other);
                break;
            }
        }
    }
    int32 Best = INDEX_NONE;
    OutHits = 0;
    for (const TPair<int32, int32>& Vote : Votes)
    {
        if (Vote.Value > OutHits)
        {
            Best = Vote.Key;
            OutHits = Vote.Value;
        }
    }
    return Best;
}

double Volume(const FBox& Box)
{
    const FVector Size = Box.GetSize();
    return Size.X * Size.Y * Size.Z;
}
} // namespace

// Side by side on the footprints turned with the actors: a chair turned toward a table it touches has a bounds' box
// that overlaps the table's.
FContact Contact(const McpBoxSat::FFootprint& A, const FBox& BoxA, const McpBoxSat::FFootprint& B, const FBox& BoxB)
{
    FContact Out;
    Out.Gap = McpBoxSat::Separation(A, B, &Out.Toward);
    const FVector2D Along(-Out.Toward.Y, Out.Toward.X);
    const double Shared = McpBoxSat::Reach(A, Along) + McpBoxSat::Reach(B, Along) - FMath::Abs(FVector2D::DotProduct(B.Center - A.Center, Along));
    const double Tall = FMath::Min(BoxA.Max.Z, BoxB.Max.Z) - FMath::Max(BoxA.Min.Z, BoxB.Min.Z);
    Out.bTouching = FMath::Abs(Out.Gap) <= TouchGap && Shared > 1.0 && Tall > 1.0;
    const bool bAlongX = FMath::Abs(Out.Toward.X) >= FMath::Abs(Out.Toward.Y);
    Out.Side = FString::Printf(TEXT("its %s%s side"), (bAlongX ? Out.Toward.X : Out.Toward.Y) > 0.0 ? TEXT("+") : TEXT("-"),
                               bAlongX ? TEXT("X") : TEXT("Y"));
    return Out;
}

void FindPairRelations(UWorld* World, const TArray<FItem>& Items, const TSet<FString>& Kinds, TArray<FRelation>& Out)
{
    const auto Wants = [&Kinds](const TCHAR* Kind) { return Kinds.Num() == 0 || Kinds.Contains(Kind); };
    TMap<const AActor*, int32> IndexOf;
    for (int32 Index = 0; Index < Items.Num(); ++Index)
    {
        IndexOf.Add(Items[Index].Actor, Index);
    }
    const FGrid Grid = BuildGrid(Items);
    // What each item rests on and what holds it from the side: a pair already joined that way is not also nextTo or faces.
    TArray<int32> Support, Mount;
    Support.Init(INDEX_NONE, Items.Num());
    Mount.Init(INDEX_NONE, Items.Num());
    for (int32 A = 0; A < Items.Num(); ++A)
    {
        int32 Hits = 0;
        Support[A] = Items[A].bGround ? INDEX_NONE : FindSupport(World, Items, IndexOf, A, Hits);
        if (Support[A] != INDEX_NONE && Wants(TEXT("on")))
        {
            Out.Add({A, TEXT("on"), Support[A], Hits >= 3 ? FString() : FString(TEXT("an edge of it rests there"))});
        }
        // Held from the side by a neighbour, by the placement checks' own pair test; the first such in item order.
        for (const int32 B : Support[A] == INDEX_NONE && !Items[A].bGround ? Grid.Near(Items[A].Box.ExpandBy(TouchGap)) : TSet<int32>())
        {
            if (B != A && (Mount[A] == INDEX_NONE || B < Mount[A]) && McpPlacement::HangsOn(Items[A].Box, Items[B].Box, TouchGap))
            {
                Mount[A] = B;
            }
        }
        if (Mount[A] != INDEX_NONE && Wants(TEXT("mounted")))
        {
            Out.Add({A, TEXT("mounted"), Mount[A], FString()});
        }
    }
    const auto Joined = [&Support, &Mount](int32 A, int32 B) {
        return Support[A] == B || Support[B] == A || Mount[A] == B || Mount[B] == A;
    };
    for (int32 A = 0; A < Items.Num(); ++A)
    {
        const FItem& Item = Items[A];
        if (Item.bGround)
        {
            continue;
        }
        const FVector Size = Item.Box.GetSize();
        const double Reach = FMath::Max(400.0, 3.0 * FMath::Max(Size.X, Size.Y));
        FVector Forward = Item.Actor->GetActorForwardVector();
        Forward.Z = 0.0;
        const bool bForward = Forward.Normalize() && Wants(TEXT("faces"));
        int32 Container = INDEX_NONE, Faced = INDEX_NONE;
        double FacedGap = Reach;
        for (const int32 B : Grid.Near(Item.Box.ExpandBy(FVector(Reach, Reach, 0.0))))
        {
            const FItem& Other = Items[B];
            if (B == A || Other.bGround)
            {
                continue;
            }
            if (Wants(TEXT("inside")) && Other.Box.ExpandBy(1.0).IsInside(Item.Box) && Volume(Other.Box) > 2.0 * Volume(Item.Box) &&
                (Container == INDEX_NONE || Volume(Other.Box) < Volume(Items[Container].Box)))
            {
                Container = B;
            }
            const FContact Side = B > A && Wants(TEXT("nextTo")) && !Joined(A, B) ? Contact(Item.Print, Item.Box, Other.Print, Other.Box) : FContact();
            if (Side.bTouching)
            {
                Out.Add({A, TEXT("nextTo"), B, FString::Printf(TEXT("touching %s"), *Side.Side)});
            }
            // The nearest by the gap between the two, so a chair faces the table before the lamp standing on it; facing as
            // expect judges it (the line ahead meets it, or points within 20 degrees of its centre).
            const double Gap = FlatGap(Item.Box, Other.Box);
            double Off = 0.0;
            if (bForward && Gap < FacedGap && !Joined(A, B) && Other.Shape != Item.Shape && McpBoxSat::Faces(Item.Print, Other.Print, FacingGrace, Off))
            {
                Faced = B;
                FacedGap = Gap;
            }
        }
        if (Container != INDEX_NONE)
        {
            Out.Add({A, TEXT("inside"), Container, FString()});
        }
        if (Faced != INDEX_NONE)
        {
            Out.Add({A, TEXT("faces"), Faced, FString::Printf(TEXT("%.0f cm away"), FacedGap)});
        }
    }
}
} // namespace McpRelations

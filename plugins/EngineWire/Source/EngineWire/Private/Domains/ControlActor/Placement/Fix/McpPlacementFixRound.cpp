// Copyright (c) 2024 EngineWire Contributors
//
// One round of fix_placement: each actor's placement check (the audit's own) gives the move that rests a sunk or floating
// actor on the surface under it and the actors it intersects; the separation solve plans the pushes on top; then every
// planned move is made at once, so no actor is judged against a neighbour halfway through moving.

#include "Domains/ControlActor/Placement/Fix/McpPlacementFix.h"

#include "Domains/ControlActor/McpActorSupport.h"
#include "Foundation/BridgeHelpers/Actors/McpActorMove.h"

namespace McpPlacementFix
{
namespace
{
constexpr int32 MaxPenetrationCalls = 4096;

// How freely an actor moves: the smaller it is, the further a push takes it.
double MoveWeight(const AActor* Actor)
{
    FVector Origin;
    FVector Extent;
    Actor->GetActorBounds(true, Origin, Extent);
    return 1.0 / FMath::Max(8.0 * Extent.X * Extent.Y * Extent.Z, 1.0);
}
} // namespace

FFinding ReadFinding(AActor* Actor, const TSet<FString>& Kinds)
{
    TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
    McpPlacement::DescribePlacement(Actor, Entry);
    FFinding Finding;
    Entry->TryGetStringField(TEXT("placementWarning"), Finding.Warning);
    double Clearance = 0.0;
    const bool bClearance = Entry->TryGetNumberField(TEXT("groundClearance"), Clearance);
    // The audit says sunk only with a suggestedLocation, and floating in its warning; either way the clearance is the move.
    const bool bSunk = Kinds.Contains(TEXT("sunk")) && Entry->HasField(TEXT("suggestedLocation"));
    const bool bFloating = Kinds.Contains(TEXT("floating")) && Finding.Warning.Contains(TEXT("floating"));
    if (bClearance && (bSunk || bFloating))
    {
        Finding.Lift = -Clearance;
        Finding.Kinds.Add(bSunk ? TEXT("sunk") : TEXT("floating"));
    }
    const TArray<TSharedPtr<FJsonValue>>* Overlaps = nullptr;
    if (Kinds.Contains(TEXT("overlapping")) && Entry->TryGetArrayField(TEXT("overlappingActors"), Overlaps) && Overlaps)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Overlaps)
        {
            const TSharedPtr<FJsonObject>* Row = nullptr;
            FString Name;
            if (Value->TryGetObject(Row) && Row && (*Row)->TryGetStringField(TEXT("actorName"), Name))
            {
                Finding.Overlaps.Add(Name);
            }
        }
        if (Finding.Overlaps.Num() > 0)
        {
            Finding.Kinds.Add(TEXT("overlapping"));
        }
    }
    return Finding;
}

void FFixRun::Skip(const AActor* Actor, const FString& Reason)
{
    const FString Name = McpActorRef(Actor);
    bool bSeen = false;
    SkippedSeen.Add(Name + Reason, &bSeen);
    if (!bSeen)
    {
        TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("actorName"), Name);
        Row->SetStringField(TEXT("reason"), Reason);
        Skipped.Add(MakeShared<FJsonValueObject>(Row));
    }
}

TArray<AActor*> FFixRun::Round(const TArray<AActor*>& Check, bool bDryRun, TFunctionRef<AActor*(const FString&)> Find,
                               TSet<AActor*>* OutFlagged)
{
    TArray<FBody> Bodies;
    TMap<AActor*, int32> Index;
    TMap<AActor*, TSet<FString>> RoundKinds;
    TArray<TPair<int32, int32>> Pairs;
    const auto BodyOf = [&](AActor* Actor)
    {
        if (const int32* Found = Index.Find(Actor))
        {
            return *Found;
        }
        FBody Body;
        Body.Actor = Actor;
        Body.InverseMass = Movers.Contains(Actor) ? MoveWeight(Actor) : 0.0;
        return Index.Add(Actor, Bodies.Add(Body));
    };
    for (AActor* Actor : Check)
    {
        const FFinding Finding = ReadFinding(Actor, Kinds);
        if (Finding.Kinds.Num() == 0)
        {
            continue;
        }
        if (OutFlagged)
        {
            OutFlagged->Add(Actor);
        }
        const int32 Self = BodyOf(Actor);
        Bodies[Self].Offset.Z += Finding.Lift;
        RoundKinds.FindOrAdd(Actor).Append(Finding.Kinds);
        for (const FString& Name : Finding.Overlaps)
        {
            AActor* Other = Find(Name);
            if (!Other || Other == Actor)
            {
                continue;
            }
            const int32 With = BodyOf(Other);
            if (!Pairs.Contains(TPair<int32, int32>(With, Self)) && !Pairs.Contains(TPair<int32, int32>(Self, With)))
            {
                Pairs.Emplace(Self, With);
                RoundKinds.FindOrAdd(Other).Add(TEXT("overlapping"));
            }
        }
    }
    const FSolveResult Solve = Separate(Bodies, Pairs, MaxPenetrationCalls - Calls);
    Calls += Solve.Calls;
    bLimitReached |= Solve.bLimitReached;
    for (const TPair<int32, int32>& Pair : Solve.Unmeasured)
    {
        Skip(Bodies[Pair.Key].Actor, FString::Printf(TEXT("its collision and that of '%s' cannot be measured against each "
            "other (instanced, skeletal or landscape bodies): separate them by hand"), *McpActorRef(Bodies[Pair.Value].Actor)));
    }
    for (const TPair<int32, int32>& Pair : Solve.Apart)
    {
        Skip(Bodies[Pair.Key].Actor, FString::Printf(TEXT("its collision does not touch '%s': only their bounding boxes "
            "overlap, so nothing was moved"), *McpActorRef(Bodies[Pair.Value].Actor)));
    }
    TArray<AActor*> Moved;
    for (const FBody& Body : Bodies)
    {
        if (Body.InverseMass <= 0.0 || Body.Offset.IsNearlyZero(0.01))
        {
            continue;
        }
        Moved.Add(Body.Actor);
        if (!Totals.Contains(Body.Actor))
        {
            Order.Add(Body.Actor);
        }
        Totals.FindOrAdd(Body.Actor) += Body.Offset;
        Fixed.FindOrAdd(Body.Actor).Append(RoundKinds.FindRef(Body.Actor));
        if (!bDryRun)
        {
            Body.Actor->Modify();
            Body.Actor->SetActorLocation(Body.Actor->GetActorLocation() + Body.Offset, false, nullptr,
                                         ETeleportType::TeleportPhysics);
            McpFinishEditorMove(Body.Actor);
            Body.Actor->MarkPackageDirty();
        }
    }
    return Moved;
}
} // namespace McpPlacementFix

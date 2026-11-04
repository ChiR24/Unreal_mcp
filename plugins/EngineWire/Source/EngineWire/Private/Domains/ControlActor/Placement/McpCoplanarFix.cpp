// Copyright (c) 2024 EngineWire Contributors
//
// control_actor fix_coplanar. Fixing z-fighting pair by pair meant a geometric judgement per pair, a few hundred of
// them in a large level; one rule covers nearly all of them. A piece lying wholly inside the other
// face is applied to it (a door on a wall, a post foot on a slab) and comes forward; a piece only partly inside is
// sunk into it (a ramp bedded in the floor, a lintel in its posts) and goes back, so the surface it is sunk into
// shows. Either way it moves a unit, which nobody sees and the depth buffer always resolves.

#include "Domains/ControlActor/Placement/McpCoplanarFaces.h"
#include "Domains/ControlActor/McpActorSupport.h"
#include "Foundation/BridgeHelpers/Actors/McpActorMove.h"

#include "ScopedTransaction.h"

namespace McpCoplanar
{
namespace
{
// Moves one actor, ending the move as the editor ends its own.
void McpCoplanarMove(AActor* Actor, const FVector& Offset)
{
    Actor->Modify();
    Actor->SetActorLocation(Actor->GetActorLocation() + Offset, false, nullptr, ETeleportType::TeleportPhysics);
    McpFinishEditorMove(Actor);
    Actor->MarkPackageDirty();
}

TSharedPtr<FJsonValue> McpCoplanarText(const FString& Text)
{
    return MakeShared<FJsonValueString>(Text);
}

// Records a skipped pair once, however many passes meet it.
void McpCoplanarSkip(TArray<TSharedPtr<FJsonValue>>& Skipped, TSet<FString>& Seen, const FString& Line)
{
    bool bSeen = false;
    Seen.Add(Line, &bSeen);
    if (!bSeen)
    {
        Skipped.Add(McpCoplanarText(Line));
    }
}

// "'Pillar' +Z face with 'Floor'", or "'Room': its WallN +X face with its WallE" for two parts of one actor.
FString McpCoplanarPairName(const TArray<FMcpCoplanarPiece>& Pieces, const FMcpCoplanarPair& Pair)
{
    const FMcpCoplanarPiece& Mine = Pieces[Pair.Piece];
    const FMcpCoplanarPiece& Theirs = Pieces[Pair.Other];
    if (Mine.Actor == Theirs.Actor)
    {
        return FString::Printf(TEXT("'%s': its %s %s face with its %s"), *McpActorRef(Mine.Actor), *Mine.Component->GetName(),
                               *DescribeDirection(Pair.Normal), *Theirs.Component->GetName());
    }
    return FString::Printf(TEXT("'%s' %s face with '%s'"), *McpActorRef(Mine.Actor), *DescribeDirection(Pair.Normal),
                           *McpActorRef(Theirs.Actor));
}
} // namespace

bool HandleFixCoplanar(UEngineWireSubsystem* Bridge, const FString& RequestId,
                       const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld)
    {
        Bridge->SendAutomationError(Socket, RequestId, TEXT("Needs the editor world with Play In Editor stopped."),
                                    TEXT("NO_WORLD"));
        return true;
    }
    FString NameFilter;
    double Distance = 1.0;
    bool bDryRun = false;
    if (Payload.IsValid())
    {
        Payload->TryGetStringField(TEXT("nameFilter"), NameFilter);
        Payload->TryGetBoolField(TEXT("dryRun"), bDryRun);
        if (Payload->TryGetNumberField(TEXT("distance"), Distance))
        {
            Distance = FMath::Clamp(Distance, 0.1, 20.0);
        }
    }

    // A pass plans every pair with each move checked against the moves before it, so the next pass normally finds
    // nothing; the fix repeats (at most four passes) for what a plan cannot see, such as a placed copy of a Blueprint
    // that keeps a part where it was. Moved actors are kept by name: fixing a Blueprint compiles it, which replaces its
    // placed actors.
    const FScopedTransaction Transaction(NSLOCTEXT("McpCoplanar", "FixCoplanar", "Fix Coplanar Faces"), !bDryRun);
    TArray<TSharedPtr<FJsonValue>> Skipped;
    TSet<FString> SkippedSeen;
    TArray<FString> Order;
    TMap<FString, FVector> Offsets;
    TMap<FString, TArray<TSharedPtr<FJsonValue>>> Pairs;
    TMap<FString, FString> PinnedBlueprints;
    TArray<TSharedPtr<FJsonValue>> BlueprintsFixed;
    TMap<FString, FString> Left; // each pair the last pass could not part, and why
    int32 PairsFound = INDEX_NONE;
    int32 Passes = 0;
    while (Passes < (bDryRun ? 1 : 4))
    {
        TArray<FMcpCoplanarPiece> Pieces = CollectPieces(World, NameFilter);
        PairsFound = PairsFound == INDEX_NONE ? FindVisiblePairs(World, Pieces, true).Num() : PairsFound;
        TArray<FMcpCoplanarPair> Targets = FindVisiblePairs(World, Pieces, false);
        PrepareParts(Pieces, Targets, PinnedBlueprints);
        if (Targets.Num() == 0)
        {
            break;
        }
        ++Passes;
        const FMcpCoplanarSettle Settle = SettleCoplanar(Pieces, Targets, Distance);
        Left.Reset();
        for (const TPair<int32, FString>& Entry : Settle.Left)
        {
            Left.Add(McpCoplanarPairName(Pieces, Targets[Entry.Key]), Entry.Value);
        }
        TMap<AActor*, FVector> ActorMoves;
        TArray<FMcpPartMove> PartMoves;
        for (const FMcpCoplanarMove& Move : Settle.Moves)
        {
            const FMcpCoplanarPair& Pair = Targets[Move.Pair];
            const FMcpCoplanarPiece& Moved = Pieces[Move.Piece];
            const FMcpCoplanarPiece& Still = Pieces[Move.Piece == Pair.Piece ? Pair.Other : Pair.Piece];
            if (Moved.Actor == Still.Actor)
            {
                PartMoves.Add({Moved.Actor, Moved.Component, Move.Delta, Still.Component->GetName()});
                continue;
            }
            const FVector* Earlier = ActorMoves.Find(Moved.Actor); // FVector() is left unset, so no FindOrAdd
            ActorMoves.Add(Moved.Actor, Earlier ? *Earlier + Move.Delta : Move.Delta);
            const FString Ref = McpActorRef(Moved.Actor);
            if (!Offsets.Contains(Ref))
            {
                Order.Add(Ref);
                Offsets.Add(Ref, FVector::ZeroVector);
            }
            Offsets[Ref] += Move.Delta;
            Pairs.FindOrAdd(Ref).Add(McpCoplanarText(FString::Printf(
                TEXT("%s face with '%s' %s"), *DescribeDirection(Pair.Normal), *McpActorRef(Still.Actor),
                FVector::DotProduct(Move.Delta, Pair.Normal) > 0.0 ? TEXT("brought forward") : TEXT("pulled back"))));
        }
        for (const TPair<AActor*, FVector>& Entry : ActorMoves)
        {
            if (!bDryRun)
            {
                McpCoplanarMove(Entry.Key, Entry.Value); // each actor once, by the sum of its moves
            }
        }
        // After the actor moves: from here on, the actors of every Blueprint that changes are new ones.
        TArray<FString> BlueprintSkips;
        MovePartsInBlueprints(PartMoves, bDryRun, PinnedBlueprints, BlueprintsFixed, BlueprintSkips);
        for (const FString& Line : BlueprintSkips)
        {
            McpCoplanarSkip(Skipped, SkippedSeen, Line);
        }
        if (Settle.Moves.Num() == 0)
        {
            break;
        }
    }
    // Every pair still found is named with why it was left: a call that ended with pairs and no word on them sent the
    // caller hunting for them by hand.
    int32 Remaining = FMath::Max(PairsFound, 0);
    if (bDryRun)
    {
        for (const TPair<FString, FString>& Entry : Left)
        {
            McpCoplanarSkip(Skipped, SkippedSeen, Entry.Key + TEXT(": ") + Entry.Value);
        }
    }
    else
    {
        const TArray<FMcpCoplanarPiece> Pieces = CollectPieces(World, NameFilter);
        Remaining = FindVisiblePairs(World, Pieces, true).Num();
        for (const FMcpCoplanarPair& Pair : FindVisiblePairs(World, Pieces, false))
        {
            const FString Name = McpCoplanarPairName(Pieces, Pair);
            const FString* Why = Left.Find(Name);
            McpCoplanarSkip(Skipped, SkippedSeen, Name + TEXT(": ") + (Why ? *Why : FString::Printf(
                TEXT("still in one plane after %d passes: move one about 2 units apart by hand"), Passes)));
        }
    }
    TArray<TSharedPtr<FJsonValue>> Moved;
    for (const FString& Ref : Order)
    {
        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("actorName"), Ref);
        Item->SetObjectField(TEXT("offset"), McpHandlerUtils::VectorToJson(Offsets[Ref]));
        Item->SetArrayField(TEXT("pairs"), Pairs.FindOrAdd(Ref));
        Moved.Add(MakeShared<FJsonValueObject>(Item));
    }

    TSharedPtr<FJsonObject> Data = McpHandlerUtils::CreateResultObject();
    Data->SetNumberField(TEXT("pairsFound"), FMath::Max(PairsFound, 0));
    Data->SetNumberField(TEXT("actorsMoved"), bDryRun ? 0 : Moved.Num());
    Data->SetNumberField(TEXT("remainingPairs"), Remaining);
    Data->SetNumberField(TEXT("passes"), Passes);
    Data->SetBoolField(TEXT("dryRun"), bDryRun);
    Data->SetArrayField(TEXT("moved"), Moved);
    Data->SetArrayField(TEXT("blueprintsFixed"), BlueprintsFixed);
    Data->SetArrayField(TEXT("skipped"), Skipped);
    Bridge->SendAutomationResponse(
        Socket, RequestId, true,
        FString::Printf(TEXT("%s %d actors and %d Blueprint parts in %d passes for %d coplanar pairs; %d pairs remain%s"),
                        bDryRun ? TEXT("Would move") : TEXT("Moved"), Moved.Num(), BlueprintsFixed.Num(), Passes,
                        FMath::Max(PairsFound, 0), Remaining, Skipped.Num() > 0 ? TEXT(" (see skipped)") : TEXT("")),
        Data, FString());
    return true;
}
} // namespace McpCoplanar

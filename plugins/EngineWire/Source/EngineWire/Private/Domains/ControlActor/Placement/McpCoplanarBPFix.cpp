// Copyright (c) 2024 EngineWire Contributors
//
// fix_coplanar for a pair inside one Blueprint actor (a brow flush with the face it sits on, a strut in its frame).
// Moving the placed actor cannot separate two of its own parts, so the pair used to be listed for a hand edit_scs
// call per Blueprint; fourteen enemy Blueprints took fourteen of them. The part moves in its Blueprint instead.

#include "Domains/ControlActor/Placement/McpCoplanarFaces.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Domains/Property/McpPropCdoComponents.h"
#include "Domains/SCS/McpSCSHandlers.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"

namespace McpCoplanar
{
namespace
{
// One Blueprint part to move, worked out while the placed actor it was found on still stands: compiling a Blueprint
// replaces its placed actors, so no actor is read once the first Blueprint has been edited.
struct FMcpBlueprintNudge
{
    FString ClassPath;
    FString BlueprintPath;
    FString Component;
    FVector Location = FVector::ZeroVector;
    TArray<FString> Off;
};

UBlueprint* McpBlueprintOf(const AActor* Actor)
{
    return Cast<UBlueprint>(Actor->GetClass()->ClassGeneratedBy);
}

// The part's template in its actor's Blueprint, or null when the Blueprint does not define it.
const USceneComponent* McpPartTemplate(const AActor* Actor, const FString& Part)
{
    UBlueprint* Blueprint = McpBlueprintOf(Actor);
    UObject* Defaults = Blueprint && Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetDefaultObject() : nullptr;
    return Defaults ? Cast<USceneComponent>(McpPropertyCdoComponents::FindCdoComponent(Blueprint, Defaults, Part, false))
                    : nullptr;
}

// Why a part cannot move on its own in its actor's Blueprint, or empty when it can.
FString McpPartPinned(const FMcpCoplanarPiece& Piece, const TMap<FString, FString>& PinnedBlueprints)
{
    const FString Part = Piece.Component->GetName();
    if (const FString* Why = PinnedBlueprints.Find(Piece.Actor->GetClass()->GetPathName()))
    {
        return *Why;
    }
    if (Piece.Component == Piece.Actor->GetRootComponent())
    {
        // Every placed actor keeps a root transform of its own, so a template edit of the root reaches none of them.
        return FString::Printf(TEXT("%s is its root, which every placed copy positions itself"), *Part);
    }
    if (!McpPartTemplate(Piece.Actor, Part))
    {
        return FString::Printf(TEXT("%s is not a part its Blueprint defines (it is added at runtime or the actor is not "
                                    "a Blueprint)"), *Part);
    }
    return FString();
}

bool McpApplyBlueprintNudge(const FMcpBlueprintNudge& Nudge, FString& OutWhy)
{
    TSharedPtr<FJsonObject> Transform = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> Location;
    Location.Add(MakeShared<FJsonValueNumber>(Nudge.Location.X));
    Location.Add(MakeShared<FJsonValueNumber>(Nudge.Location.Y));
    Location.Add(MakeShared<FJsonValueNumber>(Nudge.Location.Z));
    Transform->SetArrayField(TEXT("location"), Location);
    const TSharedPtr<FJsonObject> Result =
        FSCSHandlers::SetSCSComponentTransform(Nudge.BlueprintPath, Nudge.Component, Transform);
    bool bOk = false;
    if (!Result.IsValid() || !Result->TryGetBoolField(TEXT("success"), bOk) || !bOk)
    {
        FString Error = TEXT("no reply");
        if (Result.IsValid())
        {
            Result->TryGetStringField(TEXT("error"), Error);
        }
        OutWhy = FString::Printf(TEXT("%s: %s could not be moved (%s)"), *Nudge.BlueprintPath, *Nudge.Component, *Error);
    }
    return bOk;
}
} // namespace

void PrepareParts(TArray<FMcpCoplanarPiece>& Pieces, TArray<FMcpCoplanarPair>& Pairs,
                  const TMap<FString, FString>& PinnedBlueprints)
{
    // Every placed copy of a Blueprint holds the same pairs, and one template edit parts them in all: only the copy
    // with the largest pair is planned.
    TMap<const UClass*, const AActor*> FirstCopy;
    Pairs.RemoveAll([&Pieces, &FirstCopy](const FMcpCoplanarPair& Pair)
    {
        const AActor* Actor = Pieces[Pair.Piece].Actor;
        return Actor == Pieces[Pair.Other].Actor && McpBlueprintOf(Actor) &&
               FirstCopy.FindOrAdd(Actor->GetClass(), Actor) != Actor;
    });
    TSet<int32> Checked;
    for (const FMcpCoplanarPair& Pair : Pairs)
    {
        if (Pieces[Pair.Piece].Actor != Pieces[Pair.Other].Actor)
        {
            continue;
        }
        for (const int32 Index : {Pair.Piece, Pair.Other})
        {
            bool bSeen = false;
            Checked.Add(Index, &bSeen);
            if (!bSeen)
            {
                Pieces[Index].PinnedWhy = McpPartPinned(Pieces[Index], PinnedBlueprints);
            }
        }
    }
}

void MovePartsInBlueprints(const TArray<FMcpPartMove>& Moves, bool bDryRun, TMap<FString, FString>& PinnedBlueprints,
                           TArray<TSharedPtr<FJsonValue>>& OutDone, TArray<FString>& OutSkipped)
{
    // A part moved for several pairs takes one edit, the sum of its moves.
    TArray<FMcpBlueprintNudge> Nudges;
    TMap<const UStaticMeshComponent*, int32> ByPart;
    for (const FMcpPartMove& Move : Moves)
    {
        const FString Part = Move.Component->GetName();
        const USceneComponent* Template = McpPartTemplate(Move.Actor, Part);
        if (!Template)
        {
            continue; // PrepareParts pinned it, so no plan moves it
        }
        // The template holds the part's place under its parent, so the move is read in the parent's frame.
        const USceneComponent* Parent = Move.Component->GetAttachParent();
        const FVector LocalShift = Parent ? Parent->GetComponentTransform().InverseTransformVector(Move.Delta)
                                          : Move.Actor->GetActorTransform().InverseTransformVector(Move.Delta);
        const int32* Existing = ByPart.Find(Move.Component);
        FMcpBlueprintNudge& Nudge = Existing ? Nudges[*Existing] : Nudges.AddDefaulted_GetRef();
        if (!Existing)
        {
            ByPart.Add(Move.Component, Nudges.Num() - 1);
            Nudge.ClassPath = Move.Actor->GetClass()->GetPathName();
            Nudge.BlueprintPath = McpBlueprintOf(Move.Actor)->GetOutermost()->GetName();
            Nudge.Component = Part;
            Nudge.Location = Template->GetRelativeLocation();
        }
        Nudge.Location += LocalShift;
        Nudge.Off.AddUnique(Move.Off);
    }
    for (const FMcpBlueprintNudge& Nudge : Nudges)
    {
        // A later pass that meets a pair in this Blueprint again finds a placed copy the edit did not reach.
        FString Why;
        if (!bDryRun && !McpApplyBlueprintNudge(Nudge, Why))
        {
            OutSkipped.Add(Why);
            PinnedBlueprints.Add(Nudge.ClassPath, TEXT("its Blueprint could not be edited (see skipped)"));
            continue;
        }
        PinnedBlueprints.Add(Nudge.ClassPath, TEXT("its Blueprint was fixed by this call already, yet this placed copy "
                                                   "still has the pair: it keeps a location of its own for one of the "
                                                   "parts, so move that part with control_actor edit_component"));
        OutDone.Add(MakeShared<FJsonValueString>(FString::Printf(
            TEXT("%s: %s moved off %s, now at (%.2f, %.2f, %.2f) in every placed copy"), *Nudge.BlueprintPath,
            *Nudge.Component, *FString::Join(Nudge.Off, TEXT(", ")), Nudge.Location.X, Nudge.Location.Y,
            Nudge.Location.Z)));
    }
}
} // namespace McpCoplanar
